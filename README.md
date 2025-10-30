# Quick start on Linux

The following libraries are needed:

* libglew-dev
* libglfw3-dev
* libglm-dev

These can be install with the command:

`sudo apt install libglew-dev libglfw3-dev libglm-dev`

The OpenGL version must also be 4.3+. To verify this, use the command:

`glxinfo | grep "OpenGL version"`

---

To run the simulator from the root `/frost` directory, either:

* Use `./run.sh`
  * You may need to `chmod +x run.sh`
* Run `./bin/frost`
  * Use `make` and `make clean` to build the binary.
