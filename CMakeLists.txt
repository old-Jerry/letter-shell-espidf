idf_component_register(
    SRCS "src/shell.c"
         "src/shell_ext.c"
         "src/shell_cmd_list.c"
         "src/shell_companion.c"
         "src/log.c"


    INCLUDE_DIRS "include"
                 "src"

    REQUIRES
        driver

    LDFRAGMENTS "shell.lf"
)

target_compile_options(${COMPONENT_LIB} PRIVATE -DSHELL_CFG_USER="shell_cfg_user.h")
