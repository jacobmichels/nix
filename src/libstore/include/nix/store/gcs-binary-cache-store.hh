#pragma once

#include "nix/store/config.hh"
#include "nix/store/binary-cache-store.hh"

namespace nix {

struct GCSBinaryCacheStoreConfig : std::enable_shared_from_this<GCSBinaryCacheStoreConfig>,
                                   virtual BinaryCacheStoreConfig
{
    std::string bucketName;

    // Constructor for actual store creation
    GCSBinaryCacheStoreConfig(std::string_view bucketName, const Params & params);

    // Constructor for URI parsing during registration
    GCSBinaryCacheStoreConfig(std::string_view uriScheme, std::string_view uri, const Params & params);

    // Constructor for registration system
    explicit GCSBinaryCacheStoreConfig(const Params & params);

    const Setting<std::string> project{this, "", "project", "GCS project ID"};

    static const std::string name()
    {
        return "GCS Binary Cache Store";
    }

    static StringSet uriSchemes()
    {
        return {"gs"};
    }

    static std::optional<ExperimentalFeature> experimentalFeature()
    {
        return std::nullopt;
    }
    static std::string doc();
    ref<Store> openStore() const override;
};

}
