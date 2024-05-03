# QRetrievalAugmentedGeneration
Userinterface for LLM question-answering on PDF documents

## Overview
QRetrievalAugmentedGeneration is a Qt/C++ project designed to facilitate question-answering tasks on PDF documents. Leveraging the power of language models, particularly Mistral, and retrieval augmented generation techniques, it provides a user-friendly interface for querying PDF content.

![preview](https://github.com/CURTLab/QRetrievalAugmentedGeneration/blob/main/Preview.PNG)

## Features
* **PDF Parsing:** The project includes functionality to parse PDF documents stored in the designated data folder.
* **Question Generation:** Users can prompt questions through the interface.
* **Retrieval Augmented Generation:** The system utilizes retrieval augmented generation techniques with embedding database to provide context-aware prompts to the language model.
* **Language Model Integration:** LLM, powered by Ollama, serves as the language model for generating responses with context from the PDFs.

## Dependencies
* **Qt:** The project is built using the [Qt framework](https://www.qt.io).
* **Ollama:** The language model integration is facilitated by [Ollama](https://ollama.com/).
* **LLMs:** `Mistral`: Run ``olama run mistral`` and `nomic-embed-text`: Run ``nomic-embed-text`` in the console

## Contributing
Contributions to QRetrievalAugmentedGeneration are welcome! If you have ideas for new features, improvements, or bug fixes, feel free to open an issue or submit a pull request.

## License

This project is licensed under the MIT License.

## Acknowledgments
The code in this project is inspired by the youtube video: [Python RAG Tutorial (with Local LLMs): AI For Your PDFs](https://www.youtube.com/watch?v=2TJxpyO3ei4) from pixegami
