// app.js - JavaScript for the LLM Chatbot Interface

document.addEventListener('DOMContentLoaded', function() {
    const curriculumSelect = document.getElementById('curriculumFile');
    const promptInput = document.getElementById('promptInput');
    const sendBtn = document.getElementById('sendBtn');
    const responseText = document.getElementById('responseText');
    const responseContainer = document.getElementById('responseContainer');
    const configForm = document.getElementById('configForm');
    const debugBtn = document.getElementById('debugBtn');
    const hideDebugBtn = document.getElementById('hideDebugBtn');
    const debugCard = document.getElementById('debugCard');
    const debugText = document.getElementById('debugText');
    const clearBtn = document.getElementById('clearBtn');
    
    // Load available curriculum files
    loadCurriculumFiles();
    
    // Event listeners
    sendBtn.addEventListener('click', generateResponse);
    debugBtn.addEventListener('click', showDebug);
    hideDebugBtn.addEventListener('click', hideDebug);
    clearBtn.addEventListener('click', clearChat);
    
    // Load curriculum files from the backend
    function loadCurriculumFiles() {
        fetch('api.php/api/curricula')
            .then(response => response.json())
            .then(data => {
                curriculumSelect.innerHTML = '';
                
                if (data.files && data.files.length > 0) {
                    data.files.forEach(file => {
                        const option = document.createElement('option');
                        option.value = file;
                        option.textContent = file;
                        curriculumSelect.appendChild(option);
                    });
                    
                    // Select the first available file if vocab_model.txt exists
                    if (data.files.includes('vocab_model.txt')) {
                        curriculumSelect.value = 'vocab_model.txt';
                    } else if (data.files.includes('curriculum_bank.txt')) {
                        curriculumSelect.value = 'curriculum_bank.txt';
                    } else if (data.files.length > 0) {
                        curriculumSelect.value = data.files[0];
                    }
                } else {
                    const option = document.createElement('option');
                    option.value = '';
                    option.textContent = 'No curriculum files found';
                    curriculumSelect.appendChild(option);
                }
            })
            .catch(error => {
                console.error('Error loading curriculum files:', error);
                curriculumSelect.innerHTML = '<option value="">Error loading curriculum files</option>';
            });
    }
    
    // Generate response from the chatbot
    async function generateResponse() {
        const prompt = promptInput.value.trim();
        const curriculum = curriculumSelect.value;
        const responseLength = document.getElementById('responseLength').value;
        const temperature = document.getElementById('temperature').value;
        
        if (!prompt) {
            alert('Please enter a prompt');
            return;
        }
        
        if (!curriculum) {
            alert('Please select a curriculum file');
            return;
        }
        
        // Show loading modal
        const modalElement = document.getElementById('loadingModal');
        const modal = new bootstrap.Modal(modalElement);
        modal.show();
        
        try {
            const response = await fetch('api.php/api/chat', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({
                    prompt: prompt,
                    curriculum: curriculum,
                    responseLength: parseInt(responseLength),
                    temperature: parseFloat(temperature)
                })
            });
            
            const data = await response.json();
            
            if (response.ok) {
                // Display the response with a typing effect
                displayResponse(data.response);
            } else {
                responseText.innerHTML = '<div class="alert alert-danger">Error: ' + data.error + '</div>';
            }
        } catch (error) {
            console.error('Error generating response:', error);
            responseText.innerHTML = '<div class="alert alert-danger">Error: ' + error.message + '</div>';
        } finally {
            // Hide loading modal
            modal.hide();
        }
    }
    
    // Display response with typing effect
    function displayResponse(text) {
        responseText.innerHTML = '';
        
        // Split text into words for typing effect
        const words = text.split(/(\s+)/);
        let i = 0;
        
        const typeWriter = () => {
            if (i < words.length) {
                responseText.innerHTML += words[i];
                i++;
                setTimeout(typeWriter, 100); // Adjust typing speed here
            }
        };
        
        typeWriter();
    }
    
    // Show debug information
    function showDebug() {
        fetch('api.php/api/debug')
            .then(response => response.json())
            .then(data => {
                debugText.textContent = data.debug;
                debugCard.style.display = 'block';
            })
            .catch(error => {
                console.error('Error fetching debug info:', error);
                debugText.textContent = 'Error fetching debug information';
                debugCard.style.display = 'block';
            });
    }
    
    // Hide debug information
    function hideDebug() {
        debugCard.style.display = 'none';
    }
    
    // Clear the chat
    function clearChat() {
        promptInput.value = '';
        responseText.innerHTML = '<p class="text-muted">Response will appear here...</p>';
    }
    
    // Allow sending with Enter key (but allow Shift+Enter for new lines)
    promptInput.addEventListener('keydown', function(e) {
        if (e.key === 'Enter' && !e.shiftKey) {
            e.preventDefault();
            generateResponse();
        }
    });
});
