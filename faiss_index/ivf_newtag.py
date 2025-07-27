# -*- coding: utf-8 -*-
import faiss                   # make faiss available
import numpy as np
import sys,os
import time,math

def build_index(ids, embeddings, dimension):
    n = len(ids)
    nlist = math.floor(math.sqrt(n))
    description = "IVF" + str(nlist) + ",Flat"
    base_index = faiss.index_factory(dimension, description, faiss.METRIC_INNER_PRODUCT)
    base_index.nprobe = 40
    print("nlist: ", nlist, "nprobe: ", base_index.nprobe)
    base_index.train(embeddings)
    base_index.add_with_ids(embeddings, ids)
    return base_index

def get_file_size(file_path):
    st = os.stat(file_path)
    return st.st_size

def load_embeddigns(file_path, embedding_num, dimension):
    ids = np.zeros(embedding_num,dtype=np.int64)
    embeddings = np.zeros((embedding_num,dimension),dtype=np.float32)
    with open(file_path, "rb") as f:
        for i in range(0, embedding_num):
            binary_record = f.read(8+4*dimension)
            id = np.frombuffer(binary_record, dtype=np.int64, count=1)
            ebd = np.frombuffer(binary_record, dtype=np.float32, count=dimension, offset=8)
            #print(id,ebd)
            ids[i:i+1] = id
            embeddings[i:i+1] = ebd
    return ids, embeddings

embedding_path = sys.argv[1]
dimension = int(sys.argv[2])
index_path = sys.argv[3]

data_size = 8 + 4 * dimension
file_size = get_file_size(embedding_path)

if (file_size % data_size) != 0:
    print("file size is not multiple of ", data_size)
    exit(0)

embedding_num = file_size//data_size

start_time = time.time()
ids,embeddings = load_embeddigns(embedding_path, embedding_num, dimension)
#print(ids[0:10], embeddings[0:10])
print("load_embeddigns time cost:", time.time() - start_time, " embedding_num: ", embedding_num)

if len(ids) == 0 or len(embeddings) == 0:
    print("mid num: ", len(ids) , "ebd num: ", len(embeddings))
    exit(0)

start_time = time.time()
index = build_index(ids, embeddings, dimension)
faiss.write_index(index, index_path)
print("build_index time cost:", time.time() - start_time)
exit(1)

