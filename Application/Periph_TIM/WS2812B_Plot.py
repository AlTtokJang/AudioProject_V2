import time
import serial
import tkinter as tk

PORT = "COM4"
BAUDRATE = 2000000

WIDTH = 16
HEIGHT = 16
CHANNELS = 3

HEADER = bytes([0xAA, 0x55, 0x10, 0x10])
FRAME_SIZE = WIDTH * HEIGHT * CHANNELS
PACKET_SIZE = len(HEADER) + FRAME_SIZE

# 창 최소 크기
MIN_WINDOW_WIDTH = 480
MIN_WINDOW_HEIGHT = 480

# 처음 창 크기
INIT_WINDOW_WIDTH = 720
INIT_WINDOW_HEIGHT = 720

# PC 화면 표시용 밝기 증폭값
DISPLAY_GAIN = 50

OUTLINE_COLOR = "#101010"


def boost_color(value):
	value = int(value * DISPLAY_GAIN)

	if value > 255:
		return 255

	if value < 0:
		return 0

	return value


def main():
	ser = serial.Serial(
		port=PORT,
		baudrate=BAUDRATE,
		timeout=0
	)

	root = tk.Tk()
	root.title("WS2812B 16x16 UART Preview")
	root.configure(bg="black")
	root.geometry(f"{INIT_WINDOW_WIDTH}x{INIT_WINDOW_HEIGHT}")
	root.minsize(MIN_WINDOW_WIDTH, MIN_WINDOW_HEIGHT)

	canvas = tk.Canvas(
		root,
		bg="black",
		highlightthickness=0
	)
	canvas.pack(fill=tk.BOTH, expand=True)

	rects = []

	for y in range(HEIGHT):
		row = []

		for x in range(WIDTH):
			rect = canvas.create_rectangle(
				0,
				0,
				0,
				0,
				fill="#000000",
				outline=OUTLINE_COLOR
			)

			row.append(rect)

		rects.append(row)

	rx_buffer = bytearray()

	frame_count = 0
	fps_time = time.time()

	last_payload = bytearray(FRAME_SIZE)

	def redraw_frame(payload):
		canvas_w = canvas.winfo_width()
		canvas_h = canvas.winfo_height()

		cell_w = canvas_w / WIDTH
		cell_h = canvas_h / HEIGHT

		for y in range(HEIGHT):
			for x in range(WIDTH):
				base = (y * WIDTH + x) * 3

				r = boost_color(payload[base + 0])
				g = boost_color(payload[base + 1])
				b = boost_color(payload[base + 2])

				color = f"#{r:02x}{g:02x}{b:02x}"

				x0 = x * cell_w
				y0 = (HEIGHT - 1 - y) * cell_h
				x1 = (x + 1) * cell_w
				y1 = (HEIGHT - y) * cell_h

				canvas.coords(
					rects[y][x],
					x0,
					y0,
					x1,
					y1
				)

				canvas.itemconfig(
					rects[y][x],
					fill=color
				)

	def on_resize(event):
		redraw_frame(last_payload)

	def on_close():
		try:
			ser.close()
		except Exception:
			pass

		root.destroy()

	def update_loop():
		nonlocal rx_buffer
		nonlocal frame_count
		nonlocal fps_time
		nonlocal last_payload

		waiting = ser.in_waiting

		if waiting > 0:
			rx_buffer.extend(ser.read(waiting))

		while True:
			header_index = rx_buffer.find(HEADER)

			if header_index < 0:
				if len(rx_buffer) > PACKET_SIZE * 4:
					del rx_buffer[:-3]
				break

			if header_index > 0:
				del rx_buffer[:header_index]

			if len(rx_buffer) < PACKET_SIZE:
				break

			payload = bytes(rx_buffer[len(HEADER):PACKET_SIZE])
			del rx_buffer[:PACKET_SIZE]

			last_payload = payload
			redraw_frame(last_payload)

			frame_count += 1

		now = time.time()

		if now - fps_time >= 1.0:
			fps = frame_count / (now - fps_time)
			print(f"RX FPS: {fps:.1f}, buffer={len(rx_buffer)} bytes")

			frame_count = 0
			fps_time = now

		root.after(1, update_loop)

	root.protocol("WM_DELETE_WINDOW", on_close)
	root.bind("<Escape>", lambda event: on_close())
	canvas.bind("<Configure>", on_resize)

	root.after(1, update_loop)
	root.mainloop()


if __name__ == "__main__":
	main()