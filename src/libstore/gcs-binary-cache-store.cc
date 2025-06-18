#include "nix/store/gcs-binary-cache-store.hh"
#include "nix/store/globals.hh"
#include "nix/store/store-registration.hh"
#include <google/cloud/storage/client.h>
#include <cstdlib>

namespace gcs = ::google::cloud::storage;

namespace nix {

// Helper function to check if debug is enabled
static bool isGcsDebugEnabled()
{
    static bool checked = false;
    static bool enabled = false;
    if (!checked) {
        enabled = std::getenv("NIX_GCS_DEBUG") != nullptr;
        checked = true;
    }
    return enabled;
}

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
    if (isGcsDebugEnabled()) {
        std::cerr << "GCS constructor called with:" << std::endl;
        std::cerr << "  uriScheme='" << uriScheme << "'" << std::endl;
        std::cerr << "  uri='" << uri << "'" << std::endl;
        std::cerr << "  params.size()=" << params.size() << std::endl;
    }

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

    if (isGcsDebugEnabled()) {
        std::cerr << "GCS constructor: extracted bucket name '" << bucketName << "'" << std::endl;
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
        if (isGcsDebugEnabled()) {
            std::cerr << "GCSBinaryCacheStoreImpl created for bucket: " << config->bucketName << std::endl;
        }
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
    if (isGcsDebugEnabled()) {
        std::cerr << "GCS openStore() called with bucket: " << bucketName << std::endl;
    }
    return make_ref<GCSBinaryCacheStoreImpl>(
        ref<GCSBinaryCacheStoreConfig>(const_cast<GCSBinaryCacheStoreConfig *>(this)->shared_from_this()));
}

static bool debugStoreConfigResolution()
{
    if (!isGcsDebugEnabled())
        return true;

    std::cerr << "=== Debugging store config resolution ===" << std::endl;

    // Test parsing a GCS URI manually
    std::cerr << "Testing manual GCS URI parsing..." << std::endl;
    try {
        auto storeRef = nix::StoreReference::parse("gs://test-bucket");
        std::visit(
            nix::overloaded{
                [](const nix::StoreReference::Auto &) { std::cerr << "  Parsed as: Auto" << std::endl; },
                [](const nix::StoreReference::Specified & spec) {
                    std::cerr << "  Parsed as: Specified" << std::endl;
                    std::cerr << "    scheme: '" << spec.scheme << "'" << std::endl;
                    std::cerr << "    authority: '" << spec.authority << "'" << std::endl;
                }},
            storeRef.variant);
    } catch (const std::exception & e) {
        std::cerr << "  Error parsing: " << e.what() << std::endl;
    }

    return true;
}

// Debug output for registration - only when env var is set
static bool __attribute__((unused)) _debug_before_reg = []() {
    if (isGcsDebugEnabled()) {
        std::cerr << "About to register GCS store..." << std::endl;
    }
    return true;
}();

static RegisterStoreImplementation<GCSBinaryCacheStoreImpl::Config> regGcsBinaryCacheStore;

static bool __attribute__((unused)) _debug_after_reg = []() {
    if (isGcsDebugEnabled()) {
        std::cerr << "GCS store registration complete" << std::endl;
    }
    return true;
}();

// Debug registered stores - only when env var is set
static bool __attribute__((unused)) _debug_registration = []() {
    if (isGcsDebugEnabled()) {
        std::cerr << "GCS store: checking registered implementations..." << std::endl;
        for (const auto & [storeName, implem] : nix::Implementations::registered()) {
            std::cerr << "  Store: " << storeName << " Schemes: ";
            for (const auto & scheme : implem.uriSchemes) {
                std::cerr << scheme << " ";
            }
            std::cerr << std::endl;
        }
    }
    return true;
}();

static bool __attribute__((unused)) _debug_resolution = []() { return debugStoreConfigResolution(); }();

};
