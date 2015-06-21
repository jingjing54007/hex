#ifndef IMAGE_REF_H
#define IMAGE_REF_H

#include "hex/messaging/serialiser.h"

class Image;

struct ImageRef {
    ImageRef(): name(), image(NULL) { }
    ImageRef(std::string name): name(name), image(NULL) { }
    std::string name;
    Image *image;
};

typedef std::vector<ImageRef> ImageSeries;

inline Serialiser& operator<<(Serialiser& serialiser, const ImageRef& r) {
    serialiser << r.name;
    return serialiser;
}

inline Deserialiser& operator>>(Deserialiser& deserialiser, ImageRef& r) {
    deserialiser >> r.name;
    r.image = NULL;
    return deserialiser;
}

#endif
