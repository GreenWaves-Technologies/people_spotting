# People Spotting Demo

This is an implementation of the visual wake words challenge winner at https://github.com/mit-han-lab/VWW.

The tflite file is a converted version of the trained float model and quantization is carried out in 8 bit activations and weights using sample data from the visual wake words dataset converted to ppm format.

There are two different builds. The first builds in emulation mode where the AutoTiler generated model is compiled using the host gcc and can be run on the host with sample input. In this mode the calls to the gap SDK are aliased onto the linux API (or ignored). The model runs in a single thread so the cluster cores are not modeled. This mode is interesting for validating that the model is generating correct results and evaluating the real quantization error using the AutoTiler CNN kernels. You can launch this build using the command:

```
make -f emul.mk clean all
```

This produces a executaple file "vww\_emul" which accepts one argument, the image to run the network on:

```
./vww_emul images/COCO_val2014_000000174838_1.ppm 
```

The images have been tagged with the expected output. The \_1 at the end of a filename indicates that there is a person in the image and a \_0 indicates no person. The emul binary also dumps the tensors produced at every layer and the actual weights and biases. The AutoTiler may have changed the order of these tensors to reduce the use of 2D DMA transactions from external memory.

The second build command builds for GAP but the output can be run on a real GAP development board such as GAPUINO or on the platform simulator GVSOC. Running on GVSOC allows the generation of execution traces. In this mode performance data is generated with the number of cycles used by each layer and the overall graph and the number of MACs executed per cycle.

The build command is:

```
make clean all
```

To run on GVSOC

```
make run platform=gvsoc
```

And on a connected GAPUINO or other GAP development board:

```
make run
```

This demo is also meant to be ran with camera collected frames in realtime. To run on Gapuino board with images grubbed from a real camera and show t, you need a special camera adaptor for the galaxycore color camera, and a modification on the board to correcty power supply the camera, the output is shown to a [TFT adafruit LCD screen](https://learn.adafruit.com/adafruit-2-8-tft-touch-shield-v2) . Write an email to support@greenwaves-technologies.com to have more info on how to get the hardware.

Once you have the hardware you can uncomment the following flags in the makefile:

```
## Mute printf in source code
SILENT=1
## Enable image grub from camera and disaply output to lcd
FROM_CAMERA=1

```


and then type a:
```
make clean all run
```
