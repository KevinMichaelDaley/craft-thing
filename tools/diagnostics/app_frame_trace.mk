# Usage: make -f Makefile -f tools/diagnostics/app_frame_trace.mk \
#   build/diagnostics/native360_app_frame_trace
# Uses the application's exact cached objects and normal orderly save/quit path.
build/diagnostics/native360_app_frame_trace: tools/diagnostics/frame_trace.c $(NATIVE360_OBJ) $(RENDERER_SHADERS) build/dungeoncraft_360
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DDC_360_VIEW -DDC_GPU_CHUNK_SLOTS=96u -DDC_PRESSURE_SWEEPS=16u $< $(NATIVE360_OBJ) -o $@ $(LDLIBS) $(FRAME_TRACE_LINK)
