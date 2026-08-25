xyzfs/ - multi-user filesystem for user-pal
==========================================

Layout (durable, install-root of 00.login-signup, never session-scoped):

  xyzfs/
  ├── session.pdl               # WHO is logged in (or guest) + active avatar
  ├── bin/                      # shared ops (future)
  └── users/
      └── <uuid>/               # one tree per signed-up human
          ├── meta.txt
          ├── home/
          │   ├── avatars/<avatar_uuid>/
          │   ├── wallet.txt
          │   ├── exchange/
          │   └── net/
          └── projects/

session.pdl (machine-wide identity)
-----------------------------------
Written on login / logout by 00.login-signup ops.
Read by 01.avatar-creation when minting/opening characters.

  STATE | mode               | logged_in | guest
  STATE | user_id            | human login name
  STATE | user_uuid          | durable id
  STATE | display_name       | 
  STATE | xyzfs_path         | xyzfs/users/<uuid>
  STATE | active_avatar_uuid | last minted/opened character
  STATE | active_avatar_path | path under xyzfs or local lobby

How a user is tagged
--------------------
At Create Account, userpal_create_account.+x mints a UUID and provisions
xyzfs/users/<uuid>/. Login writes current_login.txt AND xyzfs/session.pdl.

Character creation uses session.pdl first, then current_login.txt, then
local guest fallback.
