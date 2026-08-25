import torch
import torch.nn as nn
import torchvision.transforms as transforms
from torch.utils.data import DataLoader
from torchvision.datasets import ImageFolder

# Define a simple CNN
class SimpleCNN(nn.Module):
    def __init__(self):
        super(SimpleCNN, self).__init__()
        self.conv1 = nn.Conv2d(3, 16, kernel_size=3, stride=1, padding=1)
        self.relu = nn.ReLU()
        self.pool = nn.MaxPool2d(2, 2)
        self.fc = nn.Linear(16 * 16 * 16, 10)  # Adjust based on input size

    def forward(self, x):
        x = self.pool(self.relu(self.conv1(x)))
        x = x.view(x.size(0), -1)  # Flatten
        x = self.fc(x)
        return x

# Load and preprocess data
transform = transforms.Compose([transforms.Resize((32, 32)), transforms.ToTensor()])
dataset = ImageFolder(root='.', transform=transform)
dataloader = DataLoader(dataset, batch_size=4, shuffle=True)

# Check data loading
for images, labels in dataloader:
    print(f"Batch shape: {images.shape}, Labels: {labels}")
    break  # Just check the first batch

# Initialize model, loss, and optimizer
model = SimpleCNN()
criterion = nn.CrossEntropyLoss()
optimizer = torch.optim.Adam(model.parameters(), lr=0.001)

# Training loop
for epoch in range(5):  # Few epochs for debugging
    for images, labels in dataloader:
        outputs = model(images)
        loss = criterion(outputs, labels)
        optimizer.zero_grad()
        loss.backward()
        optimizer.step()
    print(f"Epoch {epoch+1}, Loss: {loss.item()}")

# Check output
with torch.no_grad():
    sample_output = model(images)
    print(f"Sample output shape: {sample_output.shape}, Values: {sample_output[0]}")
