import tkinter as tk
from tkinter import simpledialog, messagebox, filedialog
from pathlib import Path
from datetime import datetime


class FontBitmapEditor:
	def __init__(self, root):
		self.root = root
		self.root.title("Simple Font Bitmap Editor")

		self.cols = 4
		self.rows = 16

		self.cell_w = 100
		self.cell_h = 10
		self.gap = 20

		self.grid = []
		self.rects = []

		self.on_color = "#ffd84a"
		self.off_color = "#303030"
		self.grid_color = "#777777"
		self.bg_color = "#181818"

		self._build_ui()
		self._reset_grid()

	def _build_ui(self):
		self.root.configure(bg=self.bg_color)

		top = tk.Frame(self.root, bg=self.bg_color)
		top.pack(padx=10, pady=10, fill="x")

		tk.Label(top, text="Columns", fg="white", bg=self.bg_color).grid(row=0, column=0, padx=4)
		self.cols_var = tk.StringVar(value=str(self.cols))
		tk.Entry(top, textvariable=self.cols_var, width=6).grid(row=0, column=1, padx=4)

		tk.Label(top, text="Rows", fg="white", bg=self.bg_color).grid(row=0, column=2, padx=4)
		self.rows_var = tk.StringVar(value=str(self.rows))
		tk.Entry(top, textvariable=self.rows_var, width=6).grid(row=0, column=3, padx=4)

		tk.Label(top, text="Cell W", fg="white", bg=self.bg_color).grid(row=0, column=4, padx=4)
		self.cell_w_var = tk.StringVar(value=str(self.cell_w))
		tk.Entry(top, textvariable=self.cell_w_var, width=6).grid(row=0, column=5, padx=4)

		tk.Label(top, text="Cell H", fg="white", bg=self.bg_color).grid(row=0, column=6, padx=4)
		self.cell_h_var = tk.StringVar(value=str(self.cell_h))
		tk.Entry(top, textvariable=self.cell_h_var, width=6).grid(row=0, column=7, padx=4)

		tk.Label(top, text="Gap", fg="white", bg=self.bg_color).grid(row=0, column=8, padx=4)
		self.gap_var = tk.StringVar(value=str(self.gap))
		tk.Entry(top, textvariable=self.gap_var, width=6).grid(row=0, column=9, padx=4)

		tk.Button(top, text="Apply Size", command=self.apply_size).grid(row=0, column=10, padx=8)
		tk.Button(top, text="Clear", command=self.clear_grid).grid(row=0, column=11, padx=4)
		tk.Button(top, text="Invert", command=self.invert_grid).grid(row=0, column=12, padx=4)
		tk.Button(top, text="Save TXT", command=self.save_txt).grid(row=0, column=13, padx=8)
		tk.Button(top, text="Load TXT", command=self.load_txt).grid(row=0, column=14, padx=4)

		info = tk.Frame(self.root, bg=self.bg_color)
		info.pack(padx=10, fill="x")

		self.char_name_var = tk.StringVar(value="A")
		tk.Label(info, text="Glyph name:", fg="white", bg=self.bg_color).pack(side="left")
		tk.Entry(info, textvariable=self.char_name_var, width=12).pack(side="left", padx=6)

		self.status_var = tk.StringVar(value="Left click: toggle cell | Drag: paint same state | Save creates TXT in program folder")
		tk.Label(info, textvariable=self.status_var, fg="#cccccc", bg=self.bg_color).pack(side="left", padx=12)

		self.canvas = tk.Canvas(self.root, bg=self.bg_color, highlightthickness=0)
		self.canvas.pack(padx=10, pady=10)

		bottom = tk.Frame(self.root, bg=self.bg_color)
		bottom.pack(padx=10, pady=(0, 10), fill="x")

		self.output = tk.Text(bottom, height=10, bg="#101010", fg="#dddddd", insertbackground="white")
		self.output.pack(fill="both", expand=True)

		self.paint_value = None

	def apply_size(self):
		try:
			cols = int(self.cols_var.get())
			rows = int(self.rows_var.get())
			cell_w = int(self.cell_w_var.get())
			cell_h = int(self.cell_h_var.get())
			gap = int(self.gap_var.get())

			if cols <= 0 or rows <= 0 or cell_w <= 0 or cell_h <= 0 or gap < 0:
				raise ValueError
		except ValueError:
			messagebox.showerror("Invalid value", "Columns, rows, cell width, and cell height must be positive integers. Gap must be 0 or more.")
			return

		self.cols = cols
		self.rows = rows
		self.cell_w = cell_w
		self.cell_h = cell_h
		self.gap = gap
		self._reset_grid()

	def _reset_grid(self):
		self.grid = [[0 for _ in range(self.cols)] for _ in range(self.rows)]
		self._draw_grid()
		self.update_output_preview()

	def _draw_grid(self):
		self.canvas.delete("all")
		self.rects = []

		canvas_w = self.cols * self.cell_w + max(0, self.cols - 1) * self.gap
		canvas_h = self.rows * self.cell_h + max(0, self.rows - 1) * self.gap

		self.canvas.config(width=canvas_w, height=canvas_h)

		for r in range(self.rows):
			row_rects = []
			for c in range(self.cols):
				x0 = c * (self.cell_w + self.gap)
				y0 = r * (self.cell_h + self.gap)
				x1 = x0 + self.cell_w
				y1 = y0 + self.cell_h

				rect = self.canvas.create_rectangle(
					x0,
					y0,
					x1,
					y1,
					fill=self.off_color,
					outline=self.grid_color,
					width=1,
					tags=(f"cell_{r}_{c}", "cell")
				)
				row_rects.append(rect)

				self.canvas.tag_bind(rect, "<Button-1>", lambda e, rr=r, cc=c: self.on_click(rr, cc))
				self.canvas.tag_bind(rect, "<B1-Motion>", lambda e, rr=r, cc=c: self.on_drag(rr, cc))
				self.canvas.tag_bind(rect, "<ButtonRelease-1>", self.on_release)

			self.rects.append(row_rects)

	def on_click(self, r, c):
		new_value = 0 if self.grid[r][c] else 1
		self.paint_value = new_value
		self.grid[r][c] = new_value
		self._refresh_cell(r, c)
		self.update_output_preview()

	def on_drag(self, r, c):
		if self.paint_value is None:
			return
		if self.grid[r][c] != self.paint_value:
			self.grid[r][c] = self.paint_value
			self._refresh_cell(r, c)
			self.update_output_preview()

	def on_release(self, event):
		self.paint_value = None

	def _refresh_cell(self, r, c):
		color = self.on_color if self.grid[r][c] else self.off_color
		self.canvas.itemconfig(self.rects[r][c], fill=color)

	def _refresh_all(self):
		for r in range(self.rows):
			for c in range(self.cols):
				self._refresh_cell(r, c)

	def clear_grid(self):
		for r in range(self.rows):
			for c in range(self.cols):
				self.grid[r][c] = 0
		self._refresh_all()
		self.update_output_preview()

	def invert_grid(self):
		for r in range(self.rows):
			for c in range(self.cols):
				self.grid[r][c] = 0 if self.grid[r][c] else 1
		self._refresh_all()
		self.update_output_preview()

	def get_row_strings(self):
		return ["".join("1" if self.grid[r][c] else "0" for c in range(self.cols)) for r in range(self.rows)]

	def get_column_hex(self):
		values = []
		for c in range(self.cols):
			value = 0
			for r in range(self.rows):
				if self.grid[r][c]:
					value |= (1 << r)
			width = max(4, (self.rows + 3) // 4)
			values.append(f"0x{value:0{width}X}")
		return values

	def update_output_preview(self):
		name = self.char_name_var.get().strip() or "glyph"
		rows = self.get_row_strings()
		cols_hex = self.get_column_hex()

		lines = []
		lines.append(f"name: {name}")
		lines.append(f"size: {self.cols}x{self.rows}")
		lines.append("")
		lines.append("[rows]")
		lines.extend(rows)
		lines.append("")
		lines.append("[column_hex]")
		lines.append(", ".join(cols_hex))
		lines.append("")
		lines.append("[c_array]")
		lines.append(f"{{ '{name[:1]}', {{ {', '.join(cols_hex)} }} }},")

		self.output.delete("1.0", "end")
		self.output.insert("1.0", "\n".join(lines))

	def save_txt(self):
		self.update_output_preview()

		default_name = self.char_name_var.get().strip()
		if not default_name:
			default_name = "glyph"

		safe_name = "".join(ch if ch.isalnum() or ch in ("_", "-") else "_" for ch in default_name)
		default_file = f"{safe_name}_{self.cols}x{self.rows}.txt"

		filename = simpledialog.askstring(
			"Save TXT",
			"File name:",
			initialvalue=default_file
		)

		if not filename:
			return

		if not filename.lower().endswith(".txt"):
			filename += ".txt"

		path = Path.cwd() / filename
		content = self.output.get("1.0", "end").rstrip() + "\n"

		try:
			path.write_text(content, encoding="utf-8")
		except OSError as e:
			messagebox.showerror("Save failed", str(e))
			return

		self.status_var.set(f"Saved: {path}")
		messagebox.showinfo("Saved", f"Saved to:\n{path}")

	def load_txt(self):
		path = filedialog.askopenfilename(
			title="Load TXT",
			filetypes=[("Text files", "*.txt"), ("All files", "*.*")]
		)
		if not path:
			return

		try:
			text = Path(path).read_text(encoding="utf-8")
		except OSError as e:
			messagebox.showerror("Load failed", str(e))
			return

		lines = [line.strip() for line in text.splitlines()]
		row_start = None
		size_line = None
		name_line = None

		for i, line in enumerate(lines):
			if line.startswith("name:"):
				name_line = line
			elif line.startswith("size:"):
				size_line = line
			elif line == "[rows]":
				row_start = i + 1

		if size_line:
			try:
				size_text = size_line.split(":", 1)[1].strip()
				cols_text, rows_text = size_text.lower().split("x")
				loaded_cols = int(cols_text)
				loaded_rows = int(rows_text)
			except Exception:
				messagebox.showerror("Load failed", "Invalid size field.")
				return
		else:
			messagebox.showerror("Load failed", "Missing size field.")
			return

		if row_start is None:
			messagebox.showerror("Load failed", "Missing [rows] section.")
			return

		row_data = []
		for line in lines[row_start:row_start + loaded_rows]:
			if len(line) != loaded_cols or any(ch not in "01" for ch in line):
				messagebox.showerror("Load failed", "Invalid row data.")
				return
			row_data.append(line)

		self.cols = loaded_cols
		self.rows = loaded_rows
		self.cols_var.set(str(self.cols))
		self.rows_var.set(str(self.rows))

		self.grid = [[1 if row_data[r][c] == "1" else 0 for c in range(self.cols)] for r in range(self.rows)]

		if name_line:
			self.char_name_var.set(name_line.split(":", 1)[1].strip())

		self._draw_grid()
		self._refresh_all()
		self.update_output_preview()
		self.status_var.set(f"Loaded: {path}")


def main():
	root = tk.Tk()
	app = FontBitmapEditor(root)

	# Update preview when glyph name changes.
	app.char_name_var.trace_add("write", lambda *_: app.update_output_preview())

	root.mainloop()


if __name__ == "__main__":
	main()
