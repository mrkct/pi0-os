SIZE = 64 * 1024

with open('64k', 'wb') as f:
    for i in range(SIZE // 512):
        f.write(int.to_bytes(i) * 512)
