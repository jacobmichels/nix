#pragma once

#include "nix/store/binary-cache-store.hh"

namespace nix {

struct GCSBinaryCacheStoreConfig : std::enable_shared_from_this<GCSBinaryCacheStoreConfig>,
                                   virtual BinaryCacheStoreConfig
{
    std::string bucketName;

    GCSBinaryCacheStoreConfig(std::string_view bucketName, const Params & params);

    const Setting<std::string> project{this, "", "project", "GCS project ID"};

    static const std::string name()
    {
        return "GCS Binary Cache Store";
    }
    static StringSet uriSchemes()
    {
        return {"gs"};
    }
    static std::string doc();
    ref<Store> openStore() const override;
};

}
