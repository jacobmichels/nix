#include "nix/store/gcs-binary-cache-store.hh"
#include "nix/store/globals.hh"
#include "nix/store/store-registration.hh"
#include <google/cloud/storage/client.h>
#include <cstdlib>

namespace gcs = ::google::cloud::storage;

namespace nix {

GCSBinaryCacheStoreConfig::GCSBinaryCacheStoreConfig(std::string_view bucketName, const Params & params)
    : StoreConfig(params)
    , BinaryCacheStoreConfig(params)
    , bucketName(std::string(bucketName))
{
}

GCSBinaryCacheStoreConfig::GCSBinaryCacheStoreConfig(const Params & params)
    : GCSBinaryCacheStoreConfig("", params)
{
}

GCSBinaryCacheStoreConfig::GCSBinaryCacheStoreConfig(
    std::string_view uriScheme, std::string_view uri, const Params & params)
    : StoreConfig(params)
    , BinaryCacheStoreConfig(params)
{
    // Parse the URI to extract bucket name
    // URI format: gs://bucket-name or gs://bucket-name/path
    if (uri.starts_with("gs://")) {
        auto bucketPart = uri.substr(5); // Remove "gs://"
        auto slashPos = bucketPart.find('/');
        if (slashPos != std::string::npos) {
            bucketName = std::string(bucketPart.substr(0, slashPos));
        } else {
            bucketName = std::string(bucketPart);
        }
    } else {
        // Fallback: assume the whole URI is the bucket name
        bucketName = std::string(uri);
    }

    if (bucketName.empty()) {
        throw UsageError("GCS store URI must specify a bucket name");
    }
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

static RegisterStoreImplementation<GCSBinaryCacheStoreImpl::Config> regGcsBinaryCacheStore;

};
