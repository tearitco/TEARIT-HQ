# CHTMGL Video Isolate Assumptions

- video work belongs in an isolate before it is folded back into broader media contracts
- first proof should emphasize:
  - semantic state
  - poster/fallback behavior
  - auditability
- first working lane can use isolate-owned playback if the isolate still owns:
  - transport state
  - poster generation
  - metadata
  - ASCII/GL mirrored audit outputs
- current lane uses in-Wraith frame swapping through `session/current_frame.png`
- shared media should load from `x0.parent-level-dev-env-02.01/#.media-library`
