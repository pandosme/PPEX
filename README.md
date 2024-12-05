# DetectX - PPE

Detects various objects related tp PPE (Personal Protection Equipment) used to increase safty.  This ACAP is based on [DetectX](https://github.com/pandosme/DetectX), an open-source package. The model is trained on selected images from [SH17dataset](https://github.com/ahmadmughees/sh17dataset) dataset.  

*The model trained is mainly for inspiration and has not been validated for production depolyment.  Detection quality depnds on distance to workers, lighting conditions, etc*

## Labels
Helmet, Person, Vest

*Note that the images below are based on the training data, not the actual detections*
![image3](https://raw.githubusercontent.com/pandosme/DetectX/PPE/pictures/PPE.jpg)

# Pre-requsite
- Axis Camera based on ARTPEC-8.  It is possible to build firmware for products having ARTPEC-7 and a TPU (e.g. P3255-LVE).

# User interface
The user interface is designed to validate detection and apply various filters.

## Detections
The 10 latest detections is shown in video as bounding box and table.  The events are shown in a separate table.

### Confidence
Initial filter to reduce the number of false detection. 

### Set Area of Intrest
Additional filter to reduce the number of false detection. Click button and use mouse to define an area that the center of the detection must be within.

### Set Minimum Size
Additional filter to reduce the number of false detection. Click button and use mouse to define a minimum width and height that the detection must have.

## Advanced
Additional filters to apply on the detection and output.

### Detection transition
A minumum time that the detection must be stable before an event is fired.  It define how trigger-happy the evant shall be.

### Min event state duration
The minumum event duration a detection may have.  

### Labels Processed
Enable or disable selected gestures.

## Integration
The service fires two different events targeting different use cases.  Service may monitor these event using camera event syste, ONVIF event stream and MQTT.
## Label state
A stateful event (high/low) for each detected label.  The event includes property state (true/false) and a label.  
## Labels Counter
An event fired everytime the number of different detected objects changed.  The event includes a property "json" that is a JSON object.  
Example 
```
{
  "label 1": 1,
  "label 2": 2
}
```

# History
### 2.2.0	October 19, 2024
- Initial commit. Based on DeteX version 2.2.0

### 2.2.1	November 5, 2024
- Reduced number of labels
- Trained on a different dataset based on selected images from various public datasets
