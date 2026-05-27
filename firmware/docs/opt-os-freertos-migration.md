# Planned change: TinyUSB `OPT_OS_NONE` → `OPT_OS_FREERTOS`

Status: **planned, not started.** Do this as its own isolated, hardware-verified
change — not bundled with anything else.

## Why

The most latency-critical thing this firmware does is *receiving* host position
commands every 20 ms (50 Hz). The standard-servo path has no control task: the
PWM-wrap ISR consumes `motor_map[].requested_position` directly, written by
`processMessage()` in the inbound chain.

Today (`CFG_TUSB_OS = OPT_OS_NONE`, `src/tusb_config.h`) `tud_task()` only runs
on the `usb_device_task` poll cadence (`USB_DEVICE_TASK_POLL_MS = 1 ms`), so
`tud_cdc_rx_cb` sees an inbound packet up to ~1 ms after the USB IRQ latched it
(worse if the priority-31 timer daemon is mid blocking-I²C). A consistent ~1 ms
is a harmless phase offset against a 20 ms budget; the concern is *jitter*
occasionally pushing a command past a PWM-wrap deadline (one stale servo
frame).

`OPT_OS_FREERTOS` makes the USB dcd ISR signal TinyUSB's internal FreeRTOS
queue, waking the USB task on packet arrival → RX serviced in ≈ISR + context
switch (tens of µs), decoupled from the poll cadence and the TX/telemetry tail.
This is a real-time determinism improvement, not just CPU savings.

Already done as the cheap first step (shipped separately): the inbound parse
chain was raised from priority 1 to `INCOMING_RX_TASK_PRIORITY` (5). That
removes the largest jitter source (the parse chain being starvable by
everything). `OPT_OS_FREERTOS` is the second, deeper step.

## The design problem (this is not a config flip)

The clean `OPT_OS_FREERTOS` pattern is one task doing `while (1) tud_task();`
that blocks on TinyUSB's internal event queue. But `usb_device_task` has a
**second wake source TinyUSB knows nothing about**: the
`usb_serial_outgoing_messages` queue it drains for CDC TX. Naive options all
fail:

- keep the 1 ms `vTaskDelay` poll → no benefit, just added risk;
- block only on TinyUSB's event queue → outbound CDC stalls until the next USB
  event;
- move CDC writes back to another context → reintroduces the multi-context
  hazard we eliminated (the whole point of the single-task design).

Required design: the USB task must wake on **either** a USB event **or**
"app has outbound data," while still being the sole caller of every `tud_*`.

### Approach

1. `CFG_TUSB_OS = OPT_OS_FREERTOS` in `src/tusb_config.h` (verify the TinyUSB
   FreeRTOS OSAL compiles/links under pico-sdk 2.2 + this FreeRTOS-Kernel).
2. Keep `usb_device_task` as the single owner of all `tud_*`. Restructure its
   loop to block efficiently instead of `vTaskDelay(1 ms)` polling:
   - Wait with a bounded timeout (e.g. the connection-check interval) on a
     FreeRTOS **task notification**.
   - `send_to_controller`'s USB stage (the `usb_serial_outgoing_messages`
     enqueue) additionally does `xTaskNotifyGive(usb_device_task_handle)` so a
     newly queued outbound message wakes the USB task immediately.
   - TinyUSB's own RX/event path wakes `tud_task()` via the FreeRTOS OSAL.
   - On wake: `tud_task()`, then the existing bounded TX burst-drain, then the
     periodic connection check. The bounded timeout still bounds the
     connection-check cadence and is a safety net if a notification is missed.
3. The TX burst-drain, stuck-message drop (`USB_TX_STALL_LIMIT`), and
   disconnect-discard logic carry over unchanged — they are already correct
   and non-blocking.

## The hard gotcha: USB IRQ priority

With `OPT_OS_FREERTOS` the rp2350 dcd ISR calls `FromISR` OSAL APIs. They are
only legal if `USBCTRL_IRQ` priority is **numerically ≥
`configMAX_SYSCALL_INTERRUPT_PRIORITY` (0xA0 here)** — i.e. maskable by FreeRTOS
critical sections. The pico-sdk default IRQ priority is `0x80` (above the
ceiling) → a `FromISR` call from the USB IRQ at default priority is illegal and
causes intermittent `configASSERT`/hard faults.

Action: explicitly `irq_set_priority(USBCTRL_IRQ, <value ≥ 0xA0>)` during USB
init, and confirm it is applied *after* anything that might reset it. This is
the single highest-risk item and is a timing-dependent failure, not a
log-readable one — it must be verified on hardware under load.

## Risks

- USB IRQ priority (above) — hard-fault class if wrong.
- TinyUSB FreeRTOS OSAL build/link integration under this exact pico-sdk /
  FreeRTOS-Kernel pairing.
- Missed-notification deadlock: the bounded wait timeout is the mandatory
  safety net — never wait forever on the notification alone.
- Behavior parity for the TX burst / stall-drop / disconnect-discard paths.

## Verification plan (do not skip — this is the lesson from the splice saga)

1. Build the full hw2/3/4 matrix clean.
2. Flash hw3 (or hw4) on the **production Linux host**.
3. Raw-reader capture (`tools/raw_cdc_capture.py`), splice-check = 0.
4. Soak: confirm `STATS` `OUT_DROP`/`IN_DROP` stay sane and `usb_tx_fifo_full`
   does not climb pathologically.
5. RX-latency check: instrument time from `tud_cdc_rx_cb` enqueue to
   `processMessage` (or compare position-command application jitter) before vs
   after; expect the ~1 ms poll component to disappear.
6. Stress: hammer inbound position messages while saturating outbound
   telemetry; confirm no hard faults (validates the USB IRQ priority) and no
   stalls.

## Rollback

Single revert: `CFG_TUSB_OS` back to `OPT_OS_NONE` + restore the
`vTaskDelay`-paced loop. Keep the change isolated in `tusb_config.h` + `usb.c`
so rollback is one commit.

## Sequencing

1. (done) Raise inbound parse chain to `INCOMING_RX_TASK_PRIORITY`.
2. Land + soak the single-task `OPT_OS_NONE` design in production.
3. Then this migration, isolated and hardware-verified per the plan above.
