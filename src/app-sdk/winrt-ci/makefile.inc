$(OBJ_PATH)\$(O)\merged\Microsoft.Devices.Midi2.winmd :
    -$(MKDIR) $(OBJ_PATH)\$(O)\merged
    $(MDMERGE) -i $(OBJ_PATH)\$(O) -o $(OBJ_PATH)\$(O)\merged -metadata_dir $(PROJECT_SDK_METADATA_PATH) -partial