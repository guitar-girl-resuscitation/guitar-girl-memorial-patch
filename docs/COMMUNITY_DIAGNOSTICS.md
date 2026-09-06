# Community diagnostics and advisory update prompt

The launcher retains bounded English diagnostic text independently of the save
database. Device SDK/model/ABIs and installed package version identify a report.
Android 11+ also supplies the last app-process exit reason/status when available.
No device-wide logcat, credentials, capability token or save dump is collected.

Startup exceptions include the stage, up to four causes and eight frames per
cause. Native logs are drained before displaying failure, and drain failures are
caught separately. Ten-second heartbeats describe waiting, not an invented error.
Rust reports bad startup arguments, clock, thread/runtime construction, readiness
channel, master materialization/catalog and database failures. Numeric JNI startup
codes also have descriptive hints; preceding detailed logs remain authoritative.

The Saves page exports current/previous text through Android's document picker.
It works independently of database import/export and needs no blanket storage
permission. A bounded private snapshot is updated asynchronously; Java uncaught
exceptions additionally record a bounded synchronous fatal note while preserving
the platform crash handler. Unexpected power loss or a native signal can still
lose the final lines: this is not a native tombstone collector, and cannot promise
to catch SIGSEGV or OOM from inside a dying process. Export is user-initiated,
not an automatic upload. Users should review the text before public posting.

Each cold launcher check queries the embedded HTTPS deployment, when enabled.
Only a valid higher Android version with the same application ID and signer
produces a localized, dismissible prompt (at most once per launcher controller).
Cancel/back continues startup. The positive action opens the deployment in the
device's normal browser. Redirects, identity mismatch, TLS/HTTP/timeout failures
never authorize an update and do not block offline play. An already-open prompt
is allowed to finish before Unity launches; networking itself is not a gate.
The old 24-hour availability cache no longer suppresses cold-start checks.

These changes require a newly published matched Patch/Server runtime and a
repatched APK. A deployment cannot retrofit a new launcher into already-installed
older APKs merely by updating its website.
