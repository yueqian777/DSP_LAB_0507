# Board Missing Tests Map Summary

- Map file: `C:\Users\zhangyueqian\lab8\DSP_LAB_0507\Debug\DSP_LAB_0507.map`
- Link errors: `0x0`
- Selected `.subband_l2` / backend memory lines:

```text
  DSPL2RAM              00800000   00040000  0003ec54  000013ac  RWIX
  SHDSPL2RAM            11800000   00040000  00000000  00040000  RWIX
  00800000    00800000    0003ec54   00000000    rw- .subband_l2
.subband_l2
*          0    00800000    0003ec54     UNINITIALIZED
                  00800000    00035448     user_subband_flow.obj (.subband_l2)
                  00835448    00004cac     user_subband_denoise.obj (.subband_l2)
                  0083a0f4    0000402c     user_subband_wola.obj (.subband_l2)
                  0083e120    00000860     user_subband_polyphase.obj (.subband_l2:uninit)
                  0083e980    00000204     user_subband_polyphase.obj (.subband_l2:init)
                  0083eb84    000000d0     user_subband_codec_loopback.obj (.subband_l2)
                  c00e220c    00000214     (.cinit..subband_l2.load) [load image, compression = rle]
	.subband_l2: load addr=c00e220c, load size=00000214 bytes, run addr=00800000, run size=0003ec54 bytes, compression=rle
```
