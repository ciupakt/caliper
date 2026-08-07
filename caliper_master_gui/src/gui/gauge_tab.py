"""caliper_master_gui.src.gui.gauge_tab

"Gauge" tab (GUI) – displays the last measurement in a large font.
Timestamp and angle displayed in the top-left corner with a smaller font.
Measurement centered vertically and horizontally, regardless of window size.
"""

import dearpygui.dearpygui as dpg


class GaugeTab:

    def __init__(self):
        self._last_value: str = "---"
        self._last_timestamp: str = ""
        self._last_angle: str = ""
        self._include_timestamp: bool = False
        self._include_angle: bool = False
        # Approximate height of measurement text (font_gauge = 360px).
        self._value_font_height: int = 360

    def create(self, parent: int):
        with dpg.tab(label="Gauge", parent=parent):
            with dpg.child_window(tag="gauge_root", no_scrollbar=True, border=False):
                # Top-left corner: timestamp + angle (smaller font).
                dpg.add_text("", tag="gauge_timestamp")
                dpg.add_text("", tag="gauge_angle")

                # Vertical spacer – set dynamically, centers measurement vertically.
                dpg.add_spacer(height=10, tag="gauge_vspacer")

                # Measurement – centered horizontally via dynamic `indent`.
                dpg.add_text("---", tag="gauge_value")

                if dpg.does_item_exist("font_gauge"):
                    dpg.bind_item_font("gauge_value", "font_gauge")
                if dpg.does_item_exist("font_gauge_meta"):
                    dpg.bind_item_font("gauge_timestamp", "font_gauge_meta")
                    dpg.bind_item_font("gauge_angle", "font_gauge_meta")

                if not dpg.does_item_exist("gauge_value_theme"):
                    with dpg.theme(tag="gauge_value_theme"):
                        with dpg.theme_component(dpg.mvText):
                            dpg.add_theme_color(
                                dpg.mvThemeCol_Text, (0, 200, 0, 255)
                            )
                dpg.bind_item_theme("gauge_value", "gauge_value_theme")

    def update(
        self,
        value: str,
        timestamp: str = "",
        angle: str = "",
        include_timestamp: bool = False,
        include_angle: bool = False,
    ):
        self._last_value = value
        self._last_timestamp = timestamp
        self._last_angle = angle
        self._include_timestamp = include_timestamp
        self._include_angle = include_angle
        self._refresh()

    def recenter(self):
        """Recalculate measurement centering (vertical + horizontal), e.g. after window resize."""
        if not dpg.does_item_exist("gauge_root") or not dpg.does_item_exist("gauge_value"):
            return
        try:
            avail = dpg.get_item_rect_size("gauge_root")
            avail_w = avail[0] if avail and len(avail) >= 1 else 0
            avail_h = avail[1] if avail and len(avail) >= 2 else 0
            if avail_w <= 0 or avail_h <= 0:
                return

            # Actual size of measurement text.
            vsize = dpg.get_item_rect_size("gauge_value")
            val_w = vsize[0] if vsize and len(vsize) >= 1 and vsize[0] > 0 else 0
            val_h = vsize[1] if vsize and len(vsize) >= 2 and vsize[1] > 0 else self._value_font_height

            # --- Horizontal centering (indent).
            indent = max(0, int((avail_w - val_w) / 2))
            dpg.configure_item("gauge_value", indent=indent)

            # --- Vertical centering (spacer height).
            meta_h = 0
            for tag in ("gauge_timestamp", "gauge_angle"):
                if dpg.does_item_exist(tag) and dpg.get_value(tag):
                    size = dpg.get_item_rect_size(tag)
                    if size and len(size) >= 2:
                        meta_h += size[1]

            spacer_h = max(0, int((avail_h - val_h) / 2 - meta_h - val_h * 0.20))
            if dpg.does_item_exist("gauge_vspacer"):
                dpg.configure_item("gauge_vspacer", height=spacer_h)
        except Exception:
            pass

    def _refresh(self):
        if not dpg.does_item_exist("gauge_root"):
            return

        if dpg.does_item_exist("gauge_value"):
            dpg.set_value("gauge_value", self._last_value)

        if dpg.does_item_exist("gauge_timestamp"):
            if self._include_timestamp and self._last_timestamp:
                dpg.set_value("gauge_timestamp", f"Timestamp: {self._last_timestamp}")
            else:
                dpg.set_value("gauge_timestamp", "")

        if dpg.does_item_exist("gauge_angle"):
            if self._include_angle and self._last_angle:
                dpg.set_value("gauge_angle", f"Angle: {self._last_angle}")
            else:
                dpg.set_value("gauge_angle", "")

        self.recenter()

    def clear(self):
        self._last_value = "---"
        self._last_timestamp = ""
        self._last_angle = ""
        self._refresh()
