import zlib, struct, wave
def png(fn, w, h):
    def px(x, y):
        d = ((x-w/2)**2 + (y-h/2)**2) ** .5
        return (230, 190, 40) if d < min(w, h)/4 else ((200, 30, 30) if d < min(w, h)/3 else (10, 100, 50))
    raw = b''.join(b'\x00' + b''.join(bytes(px(x, y)) for x in range(w)) for y in range(h))
    def ch(t, d): return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t+d) & 0xffffffff)
    open(fn, 'wb').write(b'\x89PNG\r\n\x1a\n' + ch(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) + ch(b'IDAT', zlib.compress(raw)) + ch(b'IEND', b''))
png('icon.png', 48, 48); png('banner.png', 256, 128)
w = wave.open('silence.wav', 'wb'); w.setnchannels(1); w.setsampwidth(2); w.setframerate(32000); w.writeframes(b'\x00\x00' * 32000 * 3); w.close()
