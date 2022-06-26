# TransModeller HILS Controller Interface

TransModeler is a powerful traffic simulator with very good modelling realism of signalized intersections.  The Hardware-in-the-loop Simulation Controller Interface (HilsCI) for TransModeler is implemented using the VTC framework.

## How to build

TransModeler is Windows software and its main API, TsmApi, is in-process COM API. Latest MSVC compiler that supports C++ 20 standard is required. 

The project is configured for CMake, and to build, you can directly do a CMake build.  Alternatively, you can load the CMake project in Visual Studio, and build from there.

The generated tsmci.dll should be put together with an accompanying tsmci.config.xml file.

## How to configure

- The simulation project must have a simulation step of 0.1 sec.  While TransModeller supports simulation step smaller than 0.1 seconds, such as 0.05, or 0.01, a step-size of 0.1 second is the value to go with hardware-in-loop simulation.

- The desired real-time factor must be set to 1.0. This is the best way to have a synchronized clock time across TransModeler simulator and the connected controller hardware.

- The tsmci.config.xml file is where to specify the mappings of _TransModeler's signals_ and _turning movements to loadswitch channels and detector channels_ in the field. Check out this [YouTube Video](https://www.youtube.com/watch?v=Y2gs03JFulw) for its specification, and the [sample configuration file](sample/tsmci.config.xml) here.