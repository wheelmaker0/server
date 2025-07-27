#pragma once
#include "test.h"
#include "faiss/Index.h"

class BruteEbdSearch : public Test{
protected:
    std::string embedding_path;
    uint32_t embedding_size;
public:
    virtual void add_options(po::options_description &desc) override{
        desc.add_options()
        ("embedding_path,e", po::value<std::string>(&embedding_path)->required(), "embedding_path")
        ("embedding_size,s", po::value<uint32_t>(&embedding_size)->default_value(64), "embedding_size");
    }
    virtual void run();
};

class FaissTest : public Test{
protected:
    std::string faiss_index_path;
    uint32_t embedding_size;
    uint32_t trigger_num;
    uint32_t recall_num_per_trigger;
    uint32_t nprobe;

public:
    virtual void add_options(po::options_description &desc) override{
        desc.add_options()
        ("faiss_index_path,i", po::value<std::string>(&faiss_index_path)->required(), "faiss_index_path")
        ("embedding_size,s", po::value<uint32_t>(&embedding_size)->default_value(64), "embedding_size")
        ("trigger_num,t", po::value<uint32_t>(&trigger_num)->default_value(1), "trigger_num")
        ("nprobe", po::value<uint32_t>(&nprobe)->default_value(1), "nprobe")
        ("recall_num_per_trigger,n", po::value<uint32_t>(&recall_num_per_trigger)->default_value(5000), "recall_num_per_trigger");
    }
    virtual void run();

    static faiss::Index *read_index(const std::string &path);
    static std::vector<float> make_rand_embedding(size_t n);
};

class MakeFaissIndex : public Test{
protected:
    std::string output_index_path;
    std::string input_embedding_path;
    uint32_t rand_embedding_num;
    uint32_t embedding_size;
    uint32_t nlist;
    std::string index_type;
    std::string describe;
public:
    virtual void add_options(po::options_description &desc) override{
        desc.add_options()
        ("output_index_path,o", po::value<std::string>(&output_index_path)->required(), "output_index_path")
        ("input_embedding_path,i", po::value<std::string>(&input_embedding_path)->default_value(""), "input_embedding_path")
        ("embedding_size,s", po::value<uint32_t>(&embedding_size)->default_value(64), "embedding_size")
        ("rand_embedding_num,n", po::value<uint32_t>(&rand_embedding_num)->default_value(1024), "rand_embedding_num")
        ("nlist", po::value<uint32_t>(&nlist)->default_value(0), "nlist")
        ("index_type", po::value<std::string>(&index_type)->default_value("IP"), "index_type")
        ("describe,d", po::value<std::string>(&describe)->default_value(""), "");
    }
    virtual void run();

private:
    bool load_binary_embedding(std::vector<int64_t> &ids, std::vector<float> &embeddings);
    bool make_rand_embedding(std::vector<int64_t> &ids, std::vector<float> &embeddings);
};

class RecallRate : public Test{
public:
    std::string embedding_path;
    std::string index_description;
    std::string index_type;
    uint32_t embedding_size;
    uint32_t task_num;
    uint32_t limit;
    uint32_t test_num;
    uint32_t nprobe;
    uint32_t efsearch;
    
    virtual void add_options(po::options_description &desc) override{
        desc.add_options()
        ("embedding_size,e", po::value<uint32_t>(&embedding_size)->default_value(64), "embedding_size")
        ("task_num,t", po::value<uint32_t>(&task_num)->default_value(1), "task number")
        ("limit,l", po::value<uint32_t>(&limit)->default_value(std::numeric_limits<uint32_t>::max()), "limit number of id")
        ("test_num,n", po::value<uint32_t>(&test_num)->default_value(100), "test_num")
        ("nprobe,p", po::value<uint32_t>(&nprobe)->default_value(0), "nprobe")
        ("efsearch,f", po::value<uint32_t>(&efsearch)->default_value(0), "efSearch")
        ("index_type", po::value<std::string>(&index_type)->default_value("ivf"), "index_type: ivf hnsw")
        ("embedding_path,i", po::value<std::string>(&embedding_path)->required(), "embedding_path")
        ("index_description,d", po::value<std::string>(&index_description)->default_value(""), "index_description");
    }
    virtual void run();

private:
    std::vector<int64_t> get_topn(const float *trigger_ebd, size_t ebd_size, const float *ebds, const int64_t *ids, size_t ebds_num, size_t n);
    float recall_at_n(const std::unordered_set<int64_t> &base, int64_t *target, size_t n);
    size_t get_empty(int64_t *target, size_t n);
    faiss::Index *make_ivf_index(const std::vector<int64_t> &ids, const std::vector<float> &embeddings, size_t embedding_size, std::string index_description);
    faiss::Index *make_hnsw_index(const std::vector<int64_t> &ids, const std::vector<float> &embeddings, size_t embedding_size, std::string index_description);
    faiss::Index *make_index(const std::vector<int64_t> &ids, const std::vector<float> &embeddings);
    void make_search_param(std::vector<std::unique_ptr<faiss::SearchParameters>> &search_parameters, std::vector<std::string> &sp_name);
};

class EmbeddingTest : public Test{
public:
    uint32_t embedding_size;
    uint32_t nprobe;
    std::string embedding_path;
    std::string index_path;
    virtual void add_options(po::options_description &desc) override{
        desc.add_options()
        ("embedding_size,s", po::value<uint32_t>(&embedding_size)->default_value(64), "embedding_size")
        ("nprobe,p", po::value<uint32_t>(&nprobe)->default_value(40), "nprobe")
        ("embedding_path,e", po::value<std::string>(&embedding_path)->default_value("/workspace/pdn/rough.data"), "embedding_path")
        ("index_path,i", po::value<std::string>(&index_path)->default_value("/workspace/pdn"), "index_path");
    }
    virtual void run();
};