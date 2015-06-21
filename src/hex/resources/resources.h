#ifndef RESOURCES_H
#define RESOURCES_H

#include "hex/messaging/serialiser.h"
#include "hex/messaging/message.h"
#include "hex/messaging/receiver.h"
#include "hex/messaging/updater.h"
#include "hex/graphics/graphics.h"
#include "hex/resources/view_def.h"


class Resources {
public:
    Resources() { }
    virtual ~Resources() { }
    void resolve_references();
    void resolve_image_series(std::vector<ImageRef>& image_series);
    bool resolve_image_ref(ImageRef& image_ref);

    TileViewDef *get_tile_view_def(const std::string& name) const;

public:
    ImageMap images;
    std::map<std::string, ImageSeries> image_series;
    TileViewDefMap tile_view_defs;
    UnitViewDefMap unit_view_defs;

    friend class ResourceLoader;
};


class ImageLoader {
public:
    ImageLoader(Resources *resources, Graphics *graphics): resources(resources), graphics(graphics) { }
    void load(const std::string& filename);

private:
    Resources *resources;
    Graphics *graphics;
};


class ResourceLoader: public MessageReceiver {
public:
    ResourceLoader(Resources *resources, ImageLoader *image_loader): resources(resources), image_loader(image_loader), last_unit_view_def(NULL) { }
    void receive(boost::shared_ptr<Message> msg);

private:
    Resources *resources;
    ImageLoader *image_loader;
    TileViewDef *last_tile_view_def;
    UnitViewDef *last_unit_view_def;
};


void load_resources(const std::string& filename, Resources& resources, Graphics *graphics);


#endif
