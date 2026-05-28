# Vendored from Xinyuan-LilyGo/LilyGo-EPD47 (https://github.com/Xinyuan-LilyGo/LilyGo-EPD47).
# Vendored on 2026-05-27 at commit 3607e73ba8b84f10e1595511894df52fb008b82a.
# Local modifications: none yet. If you patch this, note it here.

idf_component_register(
    SRC_DIRS "."
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_lcd driver esp_timer esp_hw_support
)
