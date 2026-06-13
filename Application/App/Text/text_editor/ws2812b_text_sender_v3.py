import tkinter as tk
from tkinter import messagebox
import hid

VID = 0x0483
PID = 0x5740
HID_INTERFACE = 2

MAX_TEXT_LEN = 50
MAX_COLOR = 100

BLOCKS = (
	(0xC1, 0, 15),
	(0xC2, 15, 15),
	(0xC3, 30, 15),
	(0xC4, 45, 5),
)


class TextSenderApp:
	def __init__(self, root):
		self.root = root
		self.root.title("STM32 WS2812B Text Sender")
		self.root.geometry("560x420")
		self.root.resizable(False, False)

		self.device = None
		self.target = None
		self.connected_path = None

		self.current_rgb = (100, 0, 0)
		self.last_text = ""
		self.char_colors = [(0, 0, 0) for _ in range(MAX_TEXT_LEN)]
		self.program_editing = False
		self.slider_syncing = False
		self.pending_refresh = False

		self.build_ui()
		self.set_sliders(self.current_rgb)
		self.refresh_all()
		self.scan_device()

	def build_ui(self):
		tk.Label(
			self.root,
			text="WS2812B Text Sender",
			font=("Arial", 18, "bold")
		).pack(pady=(14, 8))

		top = tk.Frame(self.root)
		top.pack(fill="x", padx=18)

		self.device_var = tk.StringVar(value="STM32 HID: 확인 중")
		tk.Label(top, textvariable=self.device_var, font=("Arial", 10)).pack(side="left")

		self.connect_button = tk.Button(top, text="STM32 연결", width=12, command=self.connect_device)
		self.connect_button.pack(side="right")

		text_frame = tk.Frame(self.root)
		text_frame.pack(fill="x", padx=18, pady=(14, 4))

		self.text_box = tk.Text(
			text_frame,
			font=("Arial", 18),
			width=35,
			height=3,
			wrap="none",
			relief="solid",
			borderwidth=1,
			insertwidth=2,
			exportselection=False,
			foreground=self.rgb_to_hex(self.current_rgb),
		)
		self.text_box.pack(fill="x")
		self.text_box.focus_set()

		self.text_box.bind("<KeyPress>", self.on_key_press)
		self.text_box.bind("<<Modified>>", self.on_text_modified)
		self.text_box.bind("<<Paste>>", self.on_paste)
		self.text_box.bind("<ButtonRelease-1>", self.on_selection_changed)
		self.text_box.bind("<B1-Motion>", self.on_selection_changed)
		self.text_box.bind("<<Selection>>", self.on_selection_changed)

		info = tk.Frame(self.root)
		info.pack(fill="x", padx=18, pady=(2, 0))

		self.counter_var = tk.StringVar(value="0 / 50")
		tk.Label(info, textvariable=self.counter_var, font=("Arial", 9)).pack(side="left")

		self.color_preview = tk.Label(info, text="현재 색", width=12, relief="solid")
		self.color_preview.pack(side="right")

		sliders = tk.LabelFrame(self.root, text="선택한 글자 색상", padx=10, pady=8)
		sliders.pack(fill="x", padx=18, pady=(14, 10))

		self.r_var = tk.IntVar(value=100)
		self.g_var = tk.IntVar(value=0)
		self.b_var = tk.IntVar(value=0)

		self.r_value = tk.StringVar(value="100")
		self.g_value = tk.StringVar(value="0")
		self.b_value = tk.StringVar(value="0")

		self.make_slider(sliders, "R", self.r_var, self.r_value, 0)
		self.make_slider(sliders, "G", self.g_var, self.g_value, 1)
		self.make_slider(sliders, "B", self.b_var, self.b_value, 2)

		buttons = tk.Frame(self.root)
		buttons.pack(pady=(14, 0))

		tk.Button(buttons, text="보내기", font=("Arial", 13), width=12, command=self.on_send).grid(row=0, column=0, padx=8)
		tk.Button(buttons, text="지우기", font=("Arial", 13), width=12, command=self.clear_text).grid(row=0, column=1, padx=8)

		self.root.bind("<Return>", lambda event: self.on_send())

	def make_slider(self, parent, label, var, text_var, row):
		tk.Label(parent, text=label, width=2, font=("Arial", 10, "bold")).grid(row=row, column=0, sticky="w")

		tk.Scale(
			parent,
			from_=0,
			to=MAX_COLOR,
			orient="horizontal",
			variable=var,
			showvalue=False,
			length=390,
			command=lambda value: self.on_slider_changed(),
		).grid(row=row, column=1, sticky="we", padx=8)

		tk.Label(parent, textvariable=text_var, width=4).grid(row=row, column=2, sticky="e")

	def find_device(self):
		devices = hid.enumerate(VID, PID)

		for item in devices:
			if item.get("interface_number") == HID_INTERFACE:
				return item

		for item in devices:
			path = item.get("path")
			path_text = path.decode(errors="ignore") if isinstance(path, bytes) else str(path)

			if "mi_02" in path_text.lower():
				return item

		return None

	def scan_device(self):
		try:
			found = self.find_device()

			if self.device is not None:
				if found is None or found.get("path") != self.connected_path:
					self.close_device()
					self.device_var.set("STM32 HID: 연결 안 됨")
				else:
					self.device_var.set("STM32 HID: 연결됨")
			else:
				self.target = found
				self.device_var.set("STM32 HID: 감지됨" if found is not None else "STM32 HID: 연결 안 됨")

		except Exception:
			self.close_device()
			self.device_var.set("STM32 HID: 연결 안 됨")

		self.root.after(1200, self.scan_device)

	def connect_device(self):
		if self.device is not None:
			self.device_var.set("STM32 HID: 연결됨")
			return

		try:
			self.target = self.find_device()

			if self.target is None:
				self.device_var.set("STM32 HID: 연결 안 됨")
				messagebox.showerror("연결 실패", "STM32 HID 장치를 찾지 못했습니다.")
				return

			self.device = hid.device()
			self.device.open_path(self.target["path"])
			self.device.set_nonblocking(True)
			self.connected_path = self.target["path"]
			self.device_var.set("STM32 HID: 연결됨")

		except Exception:
			self.close_device()
			self.device_var.set("STM32 HID: 연결 실패")
			messagebox.showerror("연결 실패", "STM32 장치를 열지 못했습니다. USB 연결 상태를 확인한 뒤 다시 시도하세요.")

	def close_device(self):
		if self.device is not None:
			try:
				self.device.close()
			except Exception:
				pass

		self.device = None
		self.connected_path = None

	def rgb_to_hex(self, rgb):
		r, g, b = rgb
		r = int(max(0, min(MAX_COLOR, r)) * 255 / MAX_COLOR)
		g = int(max(0, min(MAX_COLOR, g)) * 255 / MAX_COLOR)
		b = int(max(0, min(MAX_COLOR, b)) * 255 / MAX_COLOR)
		return f"#{r:02x}{g:02x}{b:02x}"

	def is_allowed_char(self, ch):
		return (ch.isalpha() and ch.isascii()) or ch.isdigit() or ch == " "

	def sanitize_text(self, text):
		return "".join(ch for ch in text.replace("\n", " ") if self.is_allowed_char(ch))[:MAX_TEXT_LEN]

	def get_text_raw(self):
		return self.text_box.get("1.0", "end-1c")

	def get_text(self):
		return self.sanitize_text(self.get_text_raw())

	def set_text(self, text):
		self.program_editing = True
		self.text_box.delete("1.0", "end")
		self.text_box.insert("1.0", text)
		self.text_box.edit_modified(False)
		self.program_editing = False

	def index_to_offset(self, index):
		try:
			return int(str(index).split(".")[1])
		except Exception:
			return 0

	def get_selection_range(self):
		try:
			start = self.index_to_offset(self.text_box.index("sel.first"))
			end = self.index_to_offset(self.text_box.index("sel.last"))
		except tk.TclError:
			return None, None
	
		text_len = len(self.get_text())
		start = max(0, min(text_len, start))
		end = max(0, min(text_len, end))
	
		if end <= start:
			return None, None
	
		return start, end

	def on_key_press(self, event):
		if event.keysym in ("BackSpace", "Delete", "Left", "Right", "Home", "End", "Tab", "Return"):
			return None

		if event.state & 0x04:
			return None

		if not event.char:
			return None

		if not self.is_allowed_char(event.char):
			return "break"

		if len(self.get_text()) >= MAX_TEXT_LEN:
			try:
				self.text_box.index("sel.first")
				return None
			except tk.TclError:
				return "break"

		return None

	def on_paste(self, event):
		try:
			raw = self.root.clipboard_get()
		except tk.TclError:
			return "break"

		text = self.sanitize_text(raw)

		try:
			start = self.index_to_offset(self.text_box.index("sel.first"))
			end = self.index_to_offset(self.text_box.index("sel.last"))
			self.text_box.delete("sel.first", "sel.last")
		except tk.TclError:
			start = self.index_to_offset(self.text_box.index("insert"))
			end = start

		current_len = len(self.get_text())
		available = MAX_TEXT_LEN - current_len + max(0, end - start)
		text = text[:available]

		self.text_box.insert("insert", text)
		self.update_colors_after_manual_edit(start, end, text)
		self.last_text = self.get_text()

		self.root.after_idle(self.refresh_all)
		self.text_box.edit_modified(False)
		return "break"

	def update_colors_after_manual_edit(self, start, end, inserted_text):
		old_colors = self.char_colors[:]
		new_colors = [(0, 0, 0) for _ in range(MAX_TEXT_LEN)]

		insert_len = len(inserted_text)
		remove_len = max(0, end - start)

		for i in range(min(start, MAX_TEXT_LEN)):
			new_colors[i] = old_colors[i]

		for i in range(insert_len):
			pos = start + i
			if pos < MAX_TEXT_LEN:
				new_colors[pos] = self.current_rgb

		old_tail = start + remove_len
		new_tail = start + insert_len

		while old_tail < MAX_TEXT_LEN and new_tail < MAX_TEXT_LEN:
			new_colors[new_tail] = old_colors[old_tail]
			old_tail += 1
			new_tail += 1

		self.char_colors = new_colors

	def update_colors_by_diff(self, old, new):
		old_colors = self.char_colors[:]
		new_colors = [(0, 0, 0) for _ in range(MAX_TEXT_LEN)]

		prefix = 0
		while prefix < len(old) and prefix < len(new) and old[prefix] == new[prefix]:
			prefix += 1

		old_suffix = len(old)
		new_suffix = len(new)

		while old_suffix > prefix and new_suffix > prefix and old[old_suffix - 1] == new[new_suffix - 1]:
			old_suffix -= 1
			new_suffix -= 1

		for i in range(prefix):
			new_colors[i] = old_colors[i]

		for i in range(prefix, new_suffix):
			new_colors[i] = self.current_rgb

		for old_i, new_i in zip(range(old_suffix, len(old)), range(new_suffix, len(new))):
			if new_i < MAX_TEXT_LEN:
				new_colors[new_i] = old_colors[old_i]

		self.char_colors = new_colors

	def on_text_modified(self, event=None):
		if self.program_editing:
			self.text_box.edit_modified(False)
			return

		if not self.text_box.edit_modified():
			return

		self.text_box.edit_modified(False)

		if self.pending_refresh:
			return

		self.pending_refresh = True
		self.root.after_idle(self.process_text_change)

	def process_text_change(self):
		self.pending_refresh = False

		raw = self.get_text_raw()
		text = self.sanitize_text(raw)

		if raw != text:
			insert = self.text_box.index("insert")
			self.set_text(text)
			try:
				self.text_box.mark_set("insert", insert)
			except tk.TclError:
				self.text_box.mark_set("insert", "end-1c")

		self.update_colors_by_diff(self.last_text, text)
		self.last_text = text
		self.refresh_all()

	def on_text_changed(self, event=None):
		self.process_text_change()

	def on_selection_changed(self, event=None):
		self.root.after_idle(self.sync_sliders_to_selection)

	def sync_sliders_to_selection(self):
		try:
			start = self.text_box.index("sel.first")
			end = self.text_box.index("sel.last")
		except tk.TclError:
			return

		start = int(start.split(".")[1])
		end = int(end.split(".")[1])

		if end <= start:
			return

		self.current_rgb = self.char_colors[start]
		self.set_sliders(self.current_rgb)

	def set_sliders(self, rgb):
		self.slider_syncing = True
		self.r_var.set(rgb[0])
		self.g_var.set(rgb[1])
		self.b_var.set(rgb[2])
		self.slider_syncing = False
		self.refresh_slider_labels()
		self.refresh_preview()

	def on_slider_changed(self):
		if self.slider_syncing:
			return

		self.current_rgb = (self.r_var.get(), self.g_var.get(), self.b_var.get())
		self.text_box.configure(foreground=self.rgb_to_hex(self.current_rgb))
		self.refresh_slider_labels()
		self.refresh_preview()
		self.apply_color_to_selection(self.current_rgb)

	def apply_color_to_selection(self, rgb):
		start, end = self.get_selection_range()
	
		if start is None or end is None:
			return
	
		for i in range(start, end):
			self.char_colors[i] = rgb
	
		self.refresh_text_color()

	def refresh_slider_labels(self):
		self.r_value.set(str(self.r_var.get()))
		self.g_value.set(str(self.g_var.get()))
		self.b_value.set(str(self.b_var.get()))

	def refresh_preview(self):
		self.color_preview.configure(
			bg=self.rgb_to_hex(self.current_rgb),
			fg="black" if sum(self.current_rgb) > 170 else "white",
		)

	def refresh_counter(self):
		self.counter_var.set(f"{len(self.get_text())} / {MAX_TEXT_LEN}")

	def refresh_text_color(self):
		text = self.get_text()

		for tag in self.text_box.tag_names():
			if tag.startswith("c_"):
				self.text_box.tag_delete(tag)

		for i in range(len(text)):
			rgb = self.char_colors[i]
			tag = f"c_{rgb[0]}_{rgb[1]}_{rgb[2]}"

			if tag not in self.text_box.tag_names():
				self.text_box.tag_configure(tag, foreground=self.rgb_to_hex(rgb))

			self.text_box.tag_add(tag, f"1.0 + {i} chars", f"1.0 + {i + 1} chars")

	def refresh_all(self):
		self.refresh_counter()
		self.refresh_text_color()
		self.refresh_slider_labels()
		self.refresh_preview()

	def build_slots(self):
		text = self.get_text().upper()
		slots = []

		for i in range(MAX_TEXT_LEN):
			if i < len(text):
				r, g, b = self.char_colors[i]
				slots.append((ord(text[i]) & 0xFF, r, g, b))
			else:
				slots.append((0, 0, 0, 0))

		if len(text) < MAX_TEXT_LEN:
			slots[len(text)] = (0, 0, 0, 0)

		return slots

	def write_report(self, cmd, start, count, slots):
		report = bytearray(65)
		report[0] = 0
		report[1] = cmd

		pos = 2

		for i in range(start, start + count):
			ch, r, g, b = slots[i]
			report[pos] = ch
			report[pos + 1] = r
			report[pos + 2] = g
			report[pos + 3] = b
			pos += 4

		result = self.device.write(report)

		if result != 65:
			raise RuntimeError("STM32로 데이터를 보내지 못했습니다. USB 연결을 확인하고 다시 연결하세요.")

	def on_send(self):
		if not self.get_text().strip():
			messagebox.showwarning("입력 없음", "보낼 문자열을 입력하세요.")
			return

		if self.device is None:
			messagebox.showwarning("연결 필요", "STM32 연결 버튼을 먼저 눌러주세요.")
			return

		try:
			slots = self.build_slots()

			for cmd, start, count in BLOCKS:
				self.write_report(cmd, start, count, slots)

		except Exception as exc:
			self.close_device()
			self.device_var.set("STM32 HID: 연결 끊김")
			messagebox.showerror("전송 실패", str(exc))

	def clear_text(self):
		self.set_text("")
		self.last_text = ""
		self.char_colors = [(0, 0, 0) for _ in range(MAX_TEXT_LEN)]
		self.refresh_all()


root = tk.Tk()
TextSenderApp(root)
root.mainloop()
