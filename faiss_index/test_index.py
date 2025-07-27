import faiss                   # make faiss available
import numpy as np
import sys,os
import time,math


index_file = sys.argv[1]
dimension = int(sys.argv[2])
print("index_file: ", index_file)
print("dimension: ", dimension)
index = faiss.read_index(index_file)
print(index.d)

ebds = np.random.random((1, dimension)).astype('float32')
score, id = index.search(ebds, 10)
print("score: ", score)
print("id", id)
