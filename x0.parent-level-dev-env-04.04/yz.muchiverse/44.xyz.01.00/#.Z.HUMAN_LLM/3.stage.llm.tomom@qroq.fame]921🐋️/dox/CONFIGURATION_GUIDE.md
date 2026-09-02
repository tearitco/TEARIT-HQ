# Configuration Guide for Neural Network Training

## How to Configure Training Parameters

### 1. Create a config.txt File
Create a `config.txt` file in the same directory as your vocabulary file. The trainer will automatically read this file during training.

### 2. Supported Configuration Parameters
The following parameters can be configured in `config.txt`:

- `epochs`: Number of training epochs (default: 10)
- `learning_rate`: Learning rate for Adam optimizer (default: 0.001)
- `beta1`: Beta1 parameter for Adam optimizer (default: 0.9)
- `beta2`: Beta2 parameter for Adam optimizer (default: 0.999)

### 3. Example config.txt
```
# Training Configuration
epochs=20
learning_rate=0.0005
beta1=0.85
beta2=0.995
```

### 4. Using the Configuration
When you run the trainer, it will automatically read the configuration from `config.txt`:
```bash
./trainer vocab.txt
```

If no `config.txt` file is found, the system will use default values.