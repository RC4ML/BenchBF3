import json

global_data = {}
global_id = 0
with open('C:\\Users\\ChenXuzheng\\Downloads\\yelp_dataset-2\\yelp_academic_dataset_review.json', 'r', encoding='utf-8') as json_file, open("yelp_academic_result_cool.txt","w",encoding='utf-8') as output_file:
    json_data = json_file.readline()
    while json_data is not None and json_data != "":
        json_obj = json.loads(json_data)
        now_id = 0
        if json_obj['user_id'] not in global_data:
            global_data[json_obj['user_id']] = global_id
            now_id = global_id
            global_id += 1
        else:
            now_id = global_data[json_obj['user_id']]
        output_file.write("{} {}\n".format(now_id, json_obj['cool']))
        json_data = json_file.readline()

