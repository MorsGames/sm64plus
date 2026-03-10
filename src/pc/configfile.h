#ifndef CONFIGFILE_H
#define CONFIGFILE_H

enum ConfigOptionType {
    CONFIG_TYPE_BOOL,
    CONFIG_TYPE_UINT,
    CONFIG_TYPE_SINT,
    CONFIG_TYPE_FLOAT,
    CONFIG_TYPE_SECTION
};

struct ConfigOption {
    const char *name;
    enum ConfigOptionType type;
    union {
        bool *boolValue;
        unsigned int *uintValue;
        int *sintValue;
        float *floatValue;
    };
};

void configfile_load(const char *filename);
void configfile_save(const char *filename);

#endif
