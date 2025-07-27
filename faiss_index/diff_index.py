import faiss                   # make faiss available
import numpy as np
import sys,os,json
import time,math

g_ebd = [
    0.186727,
    -0.0381293,
    0.0895766,
    -0.109347,
    0.334934,
    0.0248535,
    -0.0122748,
    -0.0294627,
    0.133739,
    -0.0586212,
    0.218922,
    -0.0851621,
    -0.120738,
    0.0961049,
    0.0855716,
    0.0605223,
    0.0146039,
    -0.0821131,
    -0.0112021,
    -0.119829,
    0.143845,
    0.0432045,
    0.132585,
    -0.0338899,
    0.265703,
    0.011241,
    0.0759903,
    0.081707,
    0.128429,
    0.111491,
    0.0260587,
    -0.31807,
    -0.0140139,
    0.187349,
    -0.0436392,
    0.288238,
    0.0484931,
    0.0697991,
    0.107211,
    0.0134701,
    0.251514,
    -0.00185302,
    -0.137731,
    0.0281288,
    0.0691176,
    0.128325,
    -0.00347528,
    0.083086,
    0.301605,
    -0.0231647,
    -0.100381,
    -0.0905031,
    0.0444459,
    -0.00329874,
    -0.138281,
    0.108438,
    0.0259065,
    0.0213424,
    0.0909402,
    -0.0103652,
    -0.159624,
    0.101952,
    -0.0345402,
    0.0483283
]

def read_json_to_numpy_from_file(file_path):
    with open(file_path, 'r') as f:
        data = json.load(f)  # 加载JSON数据
    # 假设JSON中包含浮点数列表
    np_array = np.array(data, dtype=np.float64)  # 转换为NumPy数组
    return np_array

dimension = int(sys.argv[1])
topn = int(sys.argv[2])
index_file1 = sys.argv[3]
index_file2 = sys.argv[4]
ebd_file = sys.argv[5]

index1 = faiss.read_index(index_file1)
index2 = faiss.read_index(index_file2)

ebds = read_json_to_numpy_from_file(ebd_file).reshape(1, dimension)
# print(ebds)
# ebds = np.array(g_ebd, dtype=np.float32).reshape(1, dimension)
# ebds = np.random.random((1, dimension)).astype('float32')
# print("ebds:", ebds)
score1, id1 = index1.search(ebds, topn)
score2, id2 = index2.search(ebds, topn)
intersect = np.intersect1d(id1,id2)
print(len(intersect), "/", topn)
