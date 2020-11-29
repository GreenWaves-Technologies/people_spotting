import sys
from PIL import Image
import os
import tensorflow as tf
import numpy as np

sys.path.append('./visualwakewords')
import pyvww
from pyvww.utils import VisualWakeWords

# Configurable Parameters
ANNOTATION_FILE_PATH=sys.argv[1] # '/home/rusci/Work/GWT/people_spotting/accuracy_on_vww/visualwakewords/annotations/instances_val.json'
COCO_PATH=sys.argv[2] #'/home/rusci/Dataset/coco/val2014/'
#TFLITE_MODEL_PATH=sys.argv[3]
#if TFLITE_MODEL_PATH=='':
TFLITE_MODEL_PATH = '../model/visual_wake_quant.tflite'

# Print Parameters
print("The annotation file is: ",ANNOTATION_FILE_PATH)
print("The COCO Dataset is located at: ", COCO_PATH)


# Preprocessing Function
def preprocess_image(image, scale_factor=0.875, central_crop=True, W_out=224, H_out=224):
    
    # this simply scale to the output dims
    if scale_factor is None and central_crop is False:
        image = image.resize((W_out, H_out),Image.BILINEAR)
        return image

    # compute resize dimensions based on scaling factor, then crop the center
    new_W = int(W_out / scale_factor)
    new_H = int(H_out / scale_factor)
            
    (orig_W, orig_H) = image.size
    if orig_W < orig_H:
        new_H = int(new_W*orig_H/orig_W)
    else:
        new_W = int(new_H*orig_W/orig_H)  
    
    #rezize
    image = image.resize((new_W, new_H),Image.BILINEAR)

    #if scale_factor is not None:
    # Crop the center of the image
    left = (new_W - W_out)/2
    top = (new_H - H_out)/2
    right = (new_W + W_out)/2
    bottom = (new_H + H_out)/2
    image = image.crop((left, top, right, bottom))
    
    return image

# Read VWW Dataset
vww = VisualWakeWords(ANNOTATION_FILE_PATH)
ids = list(sorted(vww.imgs.keys()))
DATASET_LEN = len(ids)
print(DATASET_LEN)

# Configura the TF Interpreter
interpreter = tf.lite.Interpreter(model_path=TFLITE_MODEL_PATH)
interpreter.allocate_tensors()

# Get input and output tensors of TFLite
input_details = interpreter.get_input_details()
output_details = interpreter.get_output_details()
print('Input details: ', input_details)
print('Output details: ', output_details)
scale, zero_point = input_details[0]['quantization']
input_shape = input_details[0]['shape']
[nb,h_tflite,w_tflite,c_tflite] = input_shape
#print('The input parameters: ', scale, zero_point, nb,h_tflite,w_tflite,c_tflite)

pred0 = 0
pred1 = 0
class0 = 0
class1 = 0
print('Going to run validation over {} samples'.format(DATASET_LEN))
for index in range(DATASET_LEN):
            
    img_id = ids[index]
    ann_ids = vww.getAnnIds(imgIds=img_id)
    if ann_ids:
        target = vww.loadAnns(ann_ids)[0]['category_id']
    else:
        target = 0

    #read image
    path = vww.loadImgs(img_id)[0]['file_name']
    #print(path)
    img = Image.open(os.path.join(COCO_PATH, path))
    if img.mode == 'L':
        img=img.convert('RGB')
    
    # preprocess
    img = preprocess_image(img, scale_factor=None, central_crop=False, W_out=w_tflite, H_out=h_tflite)
    img = np.array(img)
    #print(img.min(),img.max(),img.shape)
    
    #inference
    interpreter.set_tensor(input_details[0]['index'], img.reshape(input_details[0]['shape']))
    interpreter.invoke()
    output = interpreter.get_tensor(output_details[0]['index'])
    predicted_class = np.argmax(output)
    #print(predicted_class)
    
    # results
    #print('Prediction:', predicted_class, 'target: ', target)
    
    if target==0:
        class0 += 1
        if predicted_class == 0:
            pred0+=1
    elif target==1:
        class1 += 1
        if predicted_class == 1:
            pred1+=1
    else:
        print('Not recognized')

    accuracy_class0 = pred0/class0 if class0>0 else 0
    accuracy_class1 = pred1/class1 if class1>0 else 0
    avg_accuracy=(pred0+pred1)/(class0+class1)
    if (index > 0) and (index % 100 == 0):
        print('Processed {} of {} samples: test accuracy is {}%'.format(index,DATASET_LEN, avg_accuracy))

# Print final results
print('Class0:{}% ({}/{}) - Class1 {}% ({}/{}) '.format(100*accuracy_class0, pred0,class0,100*accuracy_class1,pred1,class1))
print("The average accuracy is: ", 100*(accuracy_class0+accuracy_class1)/2, '% or' , avg_accuracy, '%' ) 