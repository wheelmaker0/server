
#include "faiss_test.h"
#include "faiss/index_io.h"
#include "faiss/index_factory.h"
#include "faiss/IndexIVF.h"
#include "faiss/IndexHNSW.h"
#include "faiss/IndexIDMap.h"
#include "common/time_util.h"
#include "common/time_util.h"
#include "simd_util/simd_util.h"
#include "model/faiss_db.h"
#include "model/embedding_db.h"
#include <exception>
#include "bin2str.h"

bool load_binary_embedding(const std::string &embedding_path, uint32_t embedding_size, std::vector<int64_t> &ids, std::vector<float> &embeddings){
    std::ifstream ifs(embedding_path, std::ios::in | std::ios::binary);
    if(!ifs){
        std::cerr << "open embedding file failed:" << embedding_path << std::endl;
        return false;
    }
    ifs.seekg(0, std::ios::end);
    size_t file_len = ifs.tellg();
    size_t record_size = sizeof(int64_t) + sizeof(float) * (embedding_size);

    if((file_len % record_size) != 0){
        std::cerr << "embedding file size " << file_len << " is not multiple of record_size " << record_size << std::endl;
        return false;
    }
    ifs.seekg(0, std::ios::beg);
    size_t id_num = file_len/record_size;
    size_t readed = 0;
    ids.resize(id_num);
    embeddings.resize(id_num * embedding_size);
    std::cerr << "id_num: " << id_num << ", file_len: " << file_len << " ,record_size: " << record_size << std::endl;
    for(size_t i = 0; i < id_num; i++){
        ifs.read((char *) &ids[i], sizeof(int64_t));
        if(!ifs){
            std::cerr << "read embedding file " << embedding_path << " error in round " << i;
            return false;
        }
        ifs.read((char *) &embeddings[i*embedding_size], embedding_size * sizeof(float));
        if(!ifs){
            std::cerr << "read embedding file " << embedding_path << " error in round " << i;
            return false;
        }
    }
    return true;
}

std::vector<int64_t> get_topn(const float *trigger_ebd, size_t ebd_size, const float *ebds, const int64_t *ids, size_t ebds_num, size_t n){
    std::vector<std::pair<int64_t, float>> scores(ebds_num);
    for(size_t i = 0; i < ebds_num; i++){
        scores[i].first = ids[i];
        scores[i].second = simd_util::fvec_inner_product(trigger_ebd, ebds + i * ebd_size, ebd_size);
    }
    std::sort(scores.begin(), scores.end(), [](const std::pair<int64_t, float> &a, const std::pair<int64_t, float> &b){
        return a.second > b.second;
    });
    scores.resize(n);
    std::vector<int64_t> ret;
    ret.reserve(scores.size());
    for(auto &s : scores){
        ret.emplace_back(s.first);
    }
    return ret;
}

void BruteEbdSearch::run(){
    std::vector<int64_t> ids;
    std::vector<float> embeddings;
    if(!load_binary_embedding(embedding_path, embedding_size, ids, embeddings)){
        return ;
    }
    LOG(INFO) << "load finish: " << ids.size();
    std::string line;
    while(std::getline(std::cin, line)){
        std::vector<float> ebd;
        if(!common::deserialize_container(line, ebd)){
            LOG(INFO) << "wrong input: " << line;
            continue;
        }
        LOG(INFO) << common::container_2_str(ebd);
        auto ret = get_topn(ebd.data(), embedding_size, embeddings.data(), ids.data(), ids.size(), 10);
        LOG(INFO) << common::container_2_str(ret);
    }
}

faiss::Index *FaissTest::read_index(const std::string &path){
    try{
        auto ret = faiss::read_index(path.c_str(), faiss::IO_FLAG_READ_ONLY);
        return ret;
    }catch(const std::exception &e){
        LOG(ERROR) << "read index fail: " << e.what();
    }
    return nullptr;
}

std::vector<float> FaissTest::make_rand_embedding(size_t n){
    std::vector<float> ret;
    for(size_t i = 0; i < n; i++){
        ret.emplace_back(((float)rand())/1000);
    }
    return ret;
}

void FaissTest::run(){
    auto index = read_index(faiss_index_path);
    if(!index){
        return ;
    }
    std::vector<float> trigger_embedding =  make_rand_embedding(trigger_num * embedding_size);
    std::vector<float> scores(recall_num_per_trigger * trigger_num, 0.0f);
    std::vector<int64_t> ids(recall_num_per_trigger * trigger_num, 0);
    faiss::SearchParameters *sp = nullptr;

    faiss::IndexIVF *ivf_index = dynamic_cast<faiss::IndexIVF *>(index);
    if(ivf_index != nullptr){
        auto sp_ivf = new faiss::SearchParametersIVF();
        sp_ivf->nprobe = nprobe;
        sp = sp_ivf;
        std::cerr << "ivf nprobe: " << nprobe << std::endl;
    }
    common::StopWatchMs sw;
    index->search(trigger_num , trigger_embedding.data(), recall_num_per_trigger, scores.data(), ids.data(), sp);
    std::cerr << "time cost [search]: " << sw.lap() << std::endl;
    delete sp;
    delete index;
    for(size_t i = 0; i < trigger_num; i++){
        size_t empty_cnt = 0;
        for(size_t j = 0; j < recall_num_per_trigger; j++){
            auto k = i*recall_num_per_trigger + j;
            if(ids[k] < 0){
                empty_cnt++;
            }
            std::cout << ids[k] << " " << scores[k] << std::endl;
        }
        std::cerr << "trigger " << i << " empty_cnt: " << empty_cnt << std::endl;
    }
    return ;
}

bool MakeFaissIndex::load_binary_embedding(std::vector<int64_t> &ids, std::vector<float> &embeddings){
    std::ifstream ifs(input_embedding_path, std::ios::in | std::ios::binary);
    if(!ifs){
        std::cerr << "open embedding file failed:" << input_embedding_path << std::endl;
        return false;
    }
    ifs.seekg(0, std::ios::end);
    size_t file_len = ifs.tellg();
    size_t record_size = sizeof(int64_t) + sizeof(float) * (embedding_size);

    if((file_len % record_size) != 0){
        std::cerr << "embedding file size " << file_len << " is not multiple of record_size " << record_size << std::endl;
        return false;
    }
    ifs.seekg(0, std::ios::beg);
    size_t id_num = file_len/record_size;
    size_t readed = 0;
    ids.resize(id_num);
    embeddings.resize(id_num * embedding_size);
    std::cerr << "id_num: " << id_num << ", file_len: " << file_len << " ,record_size: " << record_size << std::endl;
    for(size_t i = 0; i < id_num; i++){
        ifs.read((char *) &ids[i], sizeof(int64_t));
        if(!ifs){
            std::cerr << "read embedding file " << input_embedding_path << " error in round " << i;
            return false;
        }
        ifs.read((char *) &embeddings[i*embedding_size], embedding_size * sizeof(float));
        if(!ifs){
            std::cerr << "read embedding file " << input_embedding_path << " error in round " << i;
            return false;
        }
    }
    return true;
}

bool MakeFaissIndex::make_rand_embedding(std::vector<int64_t> &ids, std::vector<float> &embeddings){
    ids.resize(rand_embedding_num);
    embeddings.resize(rand_embedding_num * embedding_size);
    for(size_t i = 0; i < rand_embedding_num; i++){
        ids[i] = i;
        for(size_t j = 0; j < embedding_size; j++){
            embeddings[i * embedding_size + j] = (float) rand() / 1000;
        }
    }
    return true;
}

void MakeFaissIndex::run(){
    std::vector<int64_t> ids;
    std::vector<float> embeddings;
    if(input_embedding_path != ""){
        if(!load_binary_embedding(ids, embeddings)){
            return ;
        }
    }
    else{
        make_rand_embedding(ids, embeddings);
    }
    if(nlist == 0){
        nlist = std::sqrt(ids.size());
    }
    if(describe == ""){
        describe = "IVF" + std::to_string(nlist)+ ",Flat";
    }
    std::string description = "IVF" + std::to_string(nlist)+ ",Flat";
    faiss::Index *index = nullptr;
    std::cerr << "desrc: " << description << " ,index_type: " << index_type << " ,ids.size(): " << ids.size() << " , embedding_size: " << embedding_size << std::endl;
    try{
        if(index_type == "L2"){
            index = index_factory(embedding_size, description.c_str(), faiss::METRIC_L2);
        }
        else{
            index = index_factory(embedding_size, description.c_str(), faiss::METRIC_INNER_PRODUCT);
        }
        if(index == nullptr){
            std::cerr << "index: nullptr\n";
            return ;
        }
        common::StopWatchMs sw;
        index->train(ids.size(), embeddings.data());
        std::cerr << "time cost [train]: " << sw.lap() << std::endl;
        index->add_with_ids(ids.size(), embeddings.data(), ids.data());
        std::cerr << "time cost [add_with_ids]: " << sw.lap() << std::endl;
        sw.lap();
        faiss::write_index(index, output_index_path.c_str());
    }catch(std::exception &e){
        std::cerr << e.what() << std::endl;
        index = nullptr;
    }
}

std::vector<int64_t> RecallRate::get_topn(const float *trigger_ebd, size_t ebd_size, const float *ebds, const int64_t *ids, size_t ebds_num, size_t n){
    std::vector<std::pair<int64_t, float>> scores(ebds_num);
    for(size_t i = 0; i < ebds_num; i++){
        scores[i].first = i;
        scores[i].second = simd_util::fvec_inner_product(trigger_ebd, ebds + i * ebd_size, ebd_size);
    }
    std::sort(scores.begin(), scores.end(), [](const std::pair<int64_t, float> &a, const std::pair<int64_t, float> &b){
        return a.second > b.second;
    });
    scores.resize(n);
    std::vector<int64_t> ret;
    ret.reserve(scores.size());
    for(auto &s : scores){
        ret.emplace_back(s.first);
    }
    return ret;
}

float RecallRate::recall_at_n(const std::unordered_set<int64_t> &base, int64_t *target, size_t n){
    float c = 0;
    for(size_t i = 0; i < n; i++){
        if(base.count(target[i])){
            c++;
        }
    }
    return c / n;
}

size_t RecallRate::get_empty(int64_t *target, size_t n){
    size_t ret = 0;
    for(size_t i = 0; i < n; i++){
        ret += (target[i] == -1);
    }
    return ret;
}

faiss::Index *RecallRate::make_ivf_index(const std::vector<int64_t> &ids, const std::vector<float> &embeddings, size_t embedding_size, std::string index_description){
    common::StopWatchMs w;
    faiss::Index *index = nullptr;
    try{
        if(index_description == ""){
            size_t nlist = std::sqrt(ids.size());
            index_description = "IVF" + std::to_string(nlist) + ",Flat";
        }
        std::cout << "index_description: " << index_description << std::endl;
        index = index_factory(embedding_size, index_description.c_str(), faiss::METRIC_INNER_PRODUCT);
        if(index == nullptr){
            std::cerr << "index_factory failed\n";
            return nullptr;
        }
        w.lap();
        index->train(ids.size(), embeddings.data());
        std::cout << "train cost: " << w.lap() << std::endl;
        index->add(ids.size(), embeddings.data());
        std::cout << "add_with_ids cost: " << w.lap() << std::endl;
    }catch(const std::exception &e){
        std::cerr << "exception: " << e.what() << std::endl;
        return nullptr;
    }
    return index;
}

faiss::Index *RecallRate::make_hnsw_index(const std::vector<int64_t> &ids, const std::vector<float> &embeddings, size_t embedding_size, std::string index_description){
    common::StopWatchMs w;
    faiss::Index *index = nullptr;
    try{
        if(index_description == ""){
            index_description = "HNSW64,Flat";
        }
        std::cout << "index_description: " << index_description << std::endl;
        index = index_factory(embedding_size, index_description.c_str(), faiss::METRIC_INNER_PRODUCT);
        if(index == nullptr){
            std::cerr << "index_factory failed\n";
            return nullptr;
        }

        //faiss::IndexIDMapTemplate<faiss::Index> *idmap_index = dynamic_cast<faiss::IndexIDMapTemplate<faiss::Index> *>(index);
        //std::cout << "type: " <<  boost::core::demangle(typeid(*(index)).name()) << std::endl;
        faiss::IndexHNSW * hnsw_index = dynamic_cast<faiss::IndexHNSW *>(index);
        if(hnsw_index == nullptr){
            std::cerr << "to IndexHNSW failed\n";
            return nullptr;
        }
        hnsw_index->hnsw.efConstruction = 64;
        std::cout << "efConstruction: " << hnsw_index->hnsw.efConstruction << std::endl;
        w.lap();
        index->add(ids.size(), embeddings.data());
        std::cout << "add_with_ids cost: " << w.lap() << std::endl;
    }catch(const std::exception &e){
        std::cerr << "exception: " << e.what() << std::endl;
        return nullptr;
    }
    return index;
}

faiss::Index *RecallRate::make_index(const std::vector<int64_t> &ids, const std::vector<float> &embeddings){
    if(index_type == "hnsw"){
        return make_hnsw_index(ids, embeddings, embedding_size, index_description);
    }
    return make_ivf_index(ids, embeddings, embedding_size, index_description);
}

void RecallRate::make_search_param(std::vector<std::unique_ptr<faiss::SearchParameters>> &search_parameters, std::vector<std::string> &sp_name){
    if(index_type == "ivf"){
        //指定了nprobe就固定一个了
        if(nprobe != 0){
            auto sp = new faiss::SearchParametersIVF();
            sp->nprobe = nprobe;
            search_parameters.emplace_back(sp);
            sp_name.emplace_back("nprobe"+std::to_string(nprobe));
        }
        else{
            for(auto &n : {5, 10, 20, 30, 40, 50}){
                auto sp = new faiss::SearchParametersIVF();
                sp->nprobe = n;
                search_parameters.emplace_back(sp);
                sp_name.emplace_back("nprobe"+std::to_string(n));
            }
        }
        return ;
    }

    if(index_type == "hnsw"){
        //指定了nprobe就固定一个了
        if(efsearch != 0){
            auto sp = new faiss::SearchParametersHNSW();
            sp->efSearch = efsearch;
            search_parameters.emplace_back(sp);
            sp_name.emplace_back("efSearch"+std::to_string(efsearch));
        }
        else{
            for(auto &n : {32, 64, 128, 256, 512, 1024}){
                auto sp = new faiss::SearchParametersHNSW();
                sp->efSearch = n;
                search_parameters.emplace_back(sp);
                sp_name.emplace_back("efSearch"+std::to_string(n));
            }
        }
        return ;
    }

    //不知道是啥，传空
    search_parameters.emplace_back(nullptr);
    sp_name.emplace_back("null");
    return ;
}

void RecallRate::run(){
    std::vector<int64_t> ids;
    std::vector<float> embeddings;
    if(!Ebd2Str::read_embedding(embedding_path, embedding_size, task_num, ids, embeddings, limit)){
        return ;
    }
    std::cout << "ids: " << ids.size() << std::endl;
    common::StopWatchMs w;
    faiss::Index *index = make_index(ids, embeddings);
    if(index == nullptr){
        return ;
    }
    std::vector<size_t> topn_list = {20000, 10000, 5000, 1000, 500, 50, 1};
    std::vector<std::unique_ptr<faiss::SearchParameters>> search_parameters;
    std::vector<std::string> sp_name;

    make_search_param(search_parameters, sp_name);

    std::vector<float> recall_rate(topn_list.size() * search_parameters.size(), 0);
    std::vector<float> time_cost(topn_list.size() * search_parameters.size(), 0);
    std::vector<float> empty_cnt(topn_list.size() * search_parameters.size(), 0);
    std::vector<float> output_scores(topn_list[0] * embedding_size);
    std::vector<int64_t> output_ids(topn_list[0]);
    test_num = std::min((size_t)test_num, ids.size());

    float brute_force = 0;
    for(size_t i = 0; i < test_num; i++){
        float *trigger_ebd = embeddings.data() + i * embedding_size;
        w.lap();
        auto base_vec = get_topn(trigger_ebd, embedding_size, embeddings.data(), ids.data(), ids.size(), topn_list[0]);
        brute_force += w.lap();
        for(size_t j = 0; j < topn_list.size(); j++){
            std::unordered_set<int64_t> base_set(base_vec.begin(), base_vec.begin() + topn_list[j]);
            for(size_t k = 0; k < search_parameters.size(); k++){
                w.lap();
                index->search(1, trigger_ebd, topn_list[j], output_scores.data(), output_ids.data(), search_parameters[k].get());
                time_cost[j*search_parameters.size() + k] += w.lap();
                recall_rate[j*search_parameters.size() + k] += recall_at_n(base_set, output_ids.data(), topn_list[j]);
                empty_cnt[j*search_parameters.size() + k] += get_empty(output_ids.data(), topn_list[j]);
            }
        }
    }

    std::cout << "brute_force time_cost: " << brute_force/test_num << std::endl;

    std::cout << "name, recall_rate, time_cost, param, empty_rate\n";
    for(size_t i = 0; i < topn_list.size(); i++){
        for(size_t j = 0; j < search_parameters.size(); j++){
            auto index = i*search_parameters.size()+j;
            auto rr = recall_rate[index]/test_num;
            auto tc = time_cost[index] /test_num;
            float ec = empty_cnt[index]/test_num;
            std::cout << "recall@" << topn_list[i] << ", " << rr << ", " <<  tc << ", " << sp_name[j] << ", " << ec/topn_list[i] << std::endl;
        }
    }
    return ;
}

void EmbeddingTest::run(){
    ModelConfig mc;
    mc.embedding_path = embedding_path;
    mc.index_path = index_path;
    mc.ebd_size = embedding_size;
    mc.faiss_nprobe = nprobe;
    mc.offline_embedding = 3;
    mc.output_tensor.resize(1);
    LOG(INFO) << "loading";
    auto embedding_db_ = std::make_shared<EmbeddingDB>(&mc);
    while(!embedding_db_->ready()){
        sleep(1);
    }
    LOG(INFO) << "ready";
    
    int64_t mid;
    while(std::cin >> mid){
        std::vector<float> ebd;
        if(!embedding_db_->get_item_embedding({mid}, ebd)){
            LOG(ERROR) << "not found " << mid;
            continue;
        }
        LOG(INFO) << "ebd: " << common::container_2_str(ebd);
        std::vector<int64_t> output_mids;
        std::vector<float> scores;
        if(embedding_db_->search(ebd, 10, output_mids, scores)){
            LOG(ERROR) << "search failed " << mid;
            continue;
        }
        LOG(INFO) << "output_mids: " << common::container_2_str(output_mids);
    }
}