# Client save-slot namespace

The current game stores some progression outside server RPC state. A server-only
slot switch therefore cannot isolate saves.

## Rule

Every slot-dependent client key is mapped to:

`ggfm/usn/<USN>/<original-key>`

The USN is immutable for the lifetime of a slot. The patch must capture it at
login and must not change it during gameplay. A pending switch is applied only
after returning to the title/login state.

## Classification

- `slot`: progression, tutorial flags, gallery, messenger, event state, claimed
  rewards, equipment, and gameplay preferences tied to a save.
- `install`: locale, graphics, volume and accessibility shared by all saves.
- `ephemeral`: caches that may be discarded and rebuilt.

Every unknown key fails closed as `slot`, because this client also stores
progress outside the `Key_User*` namespace. A key may become global only after
static and runtime evidence proves it is installation-wide and it is added to
the explicit install-key allowlist shared by the policy and native core.

The first confirmed contamination key is `Key_UserMessengerAlbumTable`.

The stock `NoticeDontShowDay` load is separately forced empty. This is not a
save-slot key: it guarantees that the localized preservation disclosure uses
the original startup notice UI once on every cold process launch.
