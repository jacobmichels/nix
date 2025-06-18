#include "nix/store/gcs-binary-cache-store.hh"
#include <google/cloud/storage/client.h>

namespace gcs = ::google::cloud::storage;

namespace nix {

GCSBinaryCacheStoreConfig::GCSBinaryCacheStoreConfig(std::string_view bucketName, const Params & params)
    : StoreConfig(params)
    , BinaryCacheStoreConfig(params)
    , bucketName(std::string(bucketName))
{
}

std::string GCSBinaryCacheStoreConfig::doc()
{
    return "GCS Binary Cache Store";
}

struct GCSBinaryCacheStoreImpl : virtual BinaryCacheStore
{
    using Config = GCSBinaryCacheStoreConfig;

    ref<Config> config;
    gcs::Client client;

    GCSBinaryCacheStoreImpl(ref<Config> config)
        : Store{*config}
        , BinaryCacheStore{*config}
        , config(config)
        , client(gcs::Client()) // Use default credentials
    {
    }

    std::string getUri() override
    {
        return "gs://" + config->bucketName;
    }

    bool fileExists(const std::string & path) override
    {
        auto metadata = client.GetObjectMetadata(config->bucketName, path);
        return metadata.ok();
    }

    void upsertFile(
        const std::string & path,
        std::shared_ptr<std::basic_iostream<char>> istream,
        const std::string & mimeType) override
    {
        auto writer = client.WriteObject(config->bucketName, path);
        writer << istream->rdbuf();
        writer.Close();
        if (!writer.metadata()) {
            throw Error("Failed to upload file '%s': %s", path, writer.metadata().status().message());
        }
    }

    void getFile(const std::string & path, Sink & sink) override
    {
        auto reader = client.ReadObject(config->bucketName, path);
        if (!reader) {
            throw NoSuchBinaryCacheFile("file '%s' does not exist", path);
        }

        std::string contents{std::istreambuf_iterator<char>{reader}, {}};
        sink(contents);
    }

    std::optional<TrustedFlag> isTrustedClient() override
    {
        return std::nullopt;
    }
};

ref<Store> GCSBinaryCacheStoreConfig::openStore() const
{
    return make_ref<GCSBinaryCacheStoreImpl>(
        ref<GCSBinaryCacheStoreConfig>(const_cast<GCSBinaryCacheStoreConfig *>(this)->shared_from_this()));
}

};
