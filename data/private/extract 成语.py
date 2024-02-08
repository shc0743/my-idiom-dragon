# extract 成语

import re  
  
# 定义一个函数来提取【】内的内容  
def extract_content(line):  
    match = re.search(r'【(.*?)】', line)  
    if match:  
        return match.group(1)  
    return None  
  
# 打开输入文件  
with open('1822.txt', 'r', encoding='utf-8') as infile:  
    # 打开输出文件  
    with open('out.txt', 'w', encoding='utf-8') as outfile:  
        # 逐行读取文件内容  
        for line in infile:  
            # 提取【】内的内容  
            content = extract_content(line)  
            # 如果提取到内容，则写入输出文件  
            if content:  
                outfile.write(content + '\n')  
  
print("内容已提取到out.txt文件中。")
print("paktc"); input()