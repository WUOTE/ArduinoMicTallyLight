# Control protocol, version 1

All transports feed one controller. Valid changes increment `revision`; repeating a mode
does not increment it or reset the animation. `source` records the latest valid mode command,
including a repeated mode. Status/ping queries do not change ownership. Reboots start at
`off`, revision zero; no mode is persisted to flash.

A changed mode starts from the last PWM duty written to the lamp. Color changes take effect
with the next lit output, and brightness joins the target over 250 ms by default. `hot`/`idle`
changes carry the breathing phase. `off` extinguishes immediately. Acknowledgements confirm
the new mode has been selected; they do not wait for a fade to finish. `pwm` may therefore be
zero or below full brightness in a successful acknowledgement, including for `red`/`white`.
Breathing repeats after the sum of the four configured durations: rise, peak hold, fall,
and floor hold. Longer holds extend the cycle without speeding up the fades. Holds do not
delay command processing. Changing between `hot` and `idle` during
a hold preserves its progress; steady-color and off commands can interrupt it immediately.

## USB

115200 baud, 8 data bits, no parity, 1 stop bit, no flow control. Commands are ASCII lines
terminated by LF; CRLF and surrounding whitespace are accepted. Mode names are case-insensitive.

```text
hot
off
status
ping
@abc123 idle
```

An optional `@id` prefix correlates an acknowledgement with its request. IDs contain 1–16
ASCII letters, digits, underscores or hyphens. They are correlation IDs, not a command
deduplication store: after another mode intervenes, resending a mode applies it again.
The PowerShell client uses one ID for its retries. Each reply is one JSON line:

```json
{"ok":true,"id":"abc123","mode":"idle","changed":true,"source":"usb","revision":1,"pwm":4095,"pwm_max":4095,"uptime_ms":2150,"wifi":"online","ip":"192.168.1.42","max_service_gap_ms":15,"dropped_replies":0}
```

`ping` returns the same status shape. An optional startup event is
`{"event":"ready","protocol":1}`. Clients should ignore unmatched IDs and events.

Invalid commands return `{"ok":false,"id":"...","error":"invalid_command"}`.
Lines hold at most 63 bytes before LF (including CR, if present). Overlong/non-ASCII/control
input is discarded through the next LF and returns `invalid_line`. A partial line idle for
one second returns `line_timeout` and is discarded through the next LF. Nothing executes
from a partial or truncated line. An empty LF can resynchronize a connection.

USB reception processes up to 64 bytes per service pass. Output uses a 1024-byte queue and
writes only available transmit capacity. If a client floods commands faster than it reads
replies, a whole reply may be dropped (`dropped_replies` counts this); light control keeps
running. Read acknowledgements or retry the current mode rather than streaming unchecked
commands. A success acknowledgement means that mode was applied at that instant; another
valid command may subsequently replace it.

## HTTP

One active request at a time, port 80, HTTP/1.0 or HTTP/1.1, CRLF headers. Each response closes
the connection. There is no keep-alive, pipelining, chunked request encoding or `100-continue`.

| Route | Behavior |
| --- | --- |
| `GET /` | Status JSON |
| `GET /status` | Status JSON |
| `POST /tally` | Apply the mode in a JSON body; return status JSON |

```http
POST /tally HTTP/1.1
Host: 192.168.1.42
Content-Type: application/json
Content-Length: 14
Authorization: Bearer your-configured-token

{"mode":"hot"}
```

Omit Authorization only if no token is configured. JSON schema is deliberately restricted
to exactly one `mode` string field. Whitespace is allowed; duplicate/unknown fields, escaped
strings, trailing content, non-string values and unknown modes are rejected. Mode values
are case-insensitive; the field name is exactly `mode`.

The request limits are 1024 total header bytes, 191 bytes per header/request line, 96 body
bytes and a 1500 ms total receive deadline, including slow/trickling senders. The timeout
starts at acceptance and never extends when more data arrives. Responses also have a
1500 ms send deadline. A client already accepted by the server may briefly occupy the one
HTTP slot until that deadline; USB continues to run.

Errors: 400 malformed input/unsupported framing, 401 missing or incorrect token, 404 unknown
route, 405 incorrect method, 408 timeout, 411 missing Content-Length on POST, 413 oversized
body, 415 incorrect Content-Type, 417 unsupported Expect, 431 oversized headers. Duplicate
Content-Length, Authorization and Content-Type headers are rejected. `Content-Length` in
responses is the exact number of body bytes; no extra newline is sent after the body.

Successful replies share the USB status schema with `id` empty. `uptime_ms` is a numeric
64-bit uptime maintained across the 32-bit Arduino clock wrap. `pwm` reports the last duty
written to the bridge, in `0..4095`, rather than measured lamp brightness. `wifi` is one of
disabled/starting/connecting/online/offline/unavailable. `ip` is cached, so status queries
do not perform network RPCs. `max_service_gap_ms` is the longest observed interval between
USB/light service passes since boot; it helps quantify stalls on real hardware.

## Changes from the legacy API

Bare USB mode names and the two main HTTP routes still work. The new Windows client requires
the new firmware's acknowledgement. The old client can send bare commands to the new firmware.
`GET /` now returns JSON. `uptime` (a string) is replaced by numeric `uptime_ms`; HTTP clients
should use `ok`, `mode`, and `changed` rather than the old success message. Invalid modes no
longer report success. USB `off` is an explicit off command, not a release of a separate
USB override. HTTP commands can always replace the current USB mode and vice versa.
