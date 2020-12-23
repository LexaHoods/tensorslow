/*
* Virtual model class that can be optimized using the ts::Optimizer class.
* Some basic neural networks models will be provided, but new ones can be
* user-defined.
*/

#include "../include/model.hpp"



	// ts::Model

template <typename T>
void ts::Model<T>::toggleOptimize(ts::Tensor<T> * tensor, bool enable) {
	wList.toggleOptimize(tensor, enable);
}



	// ts::Polynom

template <typename T>
ts::Polynom<T>::Polynom(unsigned order, std::vector<long> size) {

	// +1 for deg 0 coefficient
	for(unsigned i=0; i<order+1; i++) {
		coefficients.push_back(ts::Tensor<T>(
			Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic>()
			.setRandom(size[0], size[1]),
			&(this->wList))
		);
	}

	// Size of tensors
	nRows = size[0];
	nCols = size[1];

}



template <typename T>
void ts::Polynom<T>::toggleGlobalOptimize(bool enable) {
	for(unsigned i=0; i<coefficients.size(); i++) {
		this->wList.toggleOptimize(&(coefficients[i]), enable);
	}
}



template <typename T>
ts::Tensor<T> ts::Polynom<T>::compute(ts::Tensor<T> input) {

	// Assert input and coefficients have the same size
	for(unsigned i=0; i<coefficients.size(); i++) {
		if(
			input.getValue().cols() != coefficients[i].getValue().cols() ||
			input.getValue().rows() != coefficients[i].getValue().rows()
		) {
			return ts::Tensor<T>(Eigen::Array<T, 0, 0>(), NULL);
		}
	}


	ts::Tensor<T> result = ts::Tensor<T>(
		Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic>::Zero(
			coefficients[0].getValue().cols(),
			coefficients[0].getValue().rows()
		),
		&(this->wList)
	);

	ts::Tensor<T> element;


	// Begin computation loop
	for(unsigned i=0; i<coefficients.size(); i++) {
		// Reset element
		element = ts::Tensor<T>(
			Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic>::Zero(
				coefficients[i].getValue().cols(),
				coefficients[i].getValue().rows()
			),
			&(this->wList)
		);

		element = element + coefficients[i];

		// Compute element
		for(unsigned j=0; j<i; j++) {
			element = element * input;
		}

		// Increment result
		result = result + element;
	}

	return result;
}



template <typename T>
void ts::Polynom<T>::save(std::string filePath) {
	std::ofstream out(filePath);
	out << ts::serializeTensorsVector(coefficients);
	out.close();
}



template <typename T>
void ts::Polynom<T>::load(std::string filePath) {
	// Delete current tensors and reset wList
	coefficients = {};
	this->wList.reset();

	// Load new tensors
	std::ifstream in(filePath);
	coefficients = ts::parseTensorsVector(in, &(this->wList));
	in.close();

	// Set number of wors and cols
	if(coefficients.size() > 0) {
		nRows = coefficients[0].getValue().rows();
		nCols = coefficients[0].getValue().cols();
	} else {
		nRows = 0;
		nCols = 0;
	}
}



template <typename T>
long ts::Polynom<T>::rows() {
	return nRows;
}



template <typename T>
long ts::Polynom<T>::cols() {
	return nCols;
}



	// ts::MultiLayerPerceptron

template <typename T>
ts::MultiLayerPerceptron<T>::MultiLayerPerceptron(
	unsigned inputSize, std::vector<unsigned> layers
) {
	// Each element of the layers vector is a new layer, its value represents
	// the layer size. Values are randomly initialized between 0 and 1.

	// Make sure layers/input are not of size 0
	if(inputSize == 0) {
		return;
	}

	for(unsigned i=1; i<layers.size(); i++) {
		if(layers[i] == 0) {
			return;
		}
	}


	layers.insert(layers.begin(), inputSize);

	for(unsigned i=1; i<layers.size(); i++) {
		// Add layer i-1 -> layer i weights
		weights.push_back(ts::Tensor<T>(
			Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic>()
			.setRandom(layers[i], layers[i-1]),
			&(this->wList))
		);

		// Add layer i biases
		biases.push_back(ts::Tensor<T>(
			Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic>()
			.setRandom(layers[i], 1),
			&(this->wList))
		);
	}

}



template <typename T>
void ts::MultiLayerPerceptron<T>::toggleGlobalOptimize(bool enable) {
	for(unsigned i=0; i<weights.size(); i++) {
		this->toggleOptimize(&(weights[i]), enable);
		this->toggleOptimize(&(biases[i]), enable);
	}
}



template <typename T>
ts::Tensor<T> ts::MultiLayerPerceptron<T>::compute(ts::Tensor<T> input) {

	// Assert expected size
	if(input.getValue().rows() != weights[0].getValue().cols() ||
	input.getValue().cols() != 1) {
		return ts::Tensor<T>(Eigen::Array<T, 0, 0>(), NULL);
	}

	// Assert weights and biases vectors have the same size
	if(weights.size() != biases.size()) {
		return ts::Tensor<T>(Eigen::Array<T, 0, 0>(), NULL);
	}

	// Begin computation loop
	for(unsigned i=0; i<weights.size(); i++) {
		input = (*activationFunction)(matProd(weights[i], input) + biases[i]);
	}

	return input;
}



template <typename T>
void ts::MultiLayerPerceptron<T>::save(std::string filePath) {
	std::ofstream out(filePath);

	out << ts::serializeTensorsVector(weights);
	out << ts::serializeTensorsVector(biases);

	out.close();
}



template <typename T>
void ts::MultiLayerPerceptron<T>::load(std::string filePath) {
	// Delete current tensors and reset wList
	weights = {};
	biases = {};
	this->wList.reset();

	// Load new tensors
	std::ifstream in(filePath);

	weights = ts::parseTensorsVector(in, &(this->wList));
	biases = ts::parseTensorsVector(in, &(this->wList));

	in.close();
}



	// ts::ConvolutionalNetwork

template <typename T>
ts::ConvolutionalNetwork<T>::ConvolutionalNetwork(
	std::vector<unsigned> inputSize,
	std::vector<std::vector<unsigned>> convLayers,
	std::vector<unsigned> poolingSize,
	std::vector<unsigned> denseLayers
) {
	// inputSize : std::vector of size 2 for dimensions of 2D image / matrix
	// convLayers : sizes of convolution kernels (std::vector of dimension 2)
	// fullLayers: sizes of fully connected layers


		// Validate dimensions of network

	if(inputSize.size() != 2) {
		return;
	}
	if(inputSize[0] == 0 || inputSize[1] == 0) {
		return;
	}


	std::vector<int> intermediarySize = {(int) inputSize[0], (int) inputSize[1]};

	// Make sure convolutions are possible
	for(unsigned i=0; i<convLayers.size(); i++) {
		// Is size of kernel correctly described
		if(convLayers[i].size() != 2) {
			return;
		}
		if(convLayers[i][0] == 0 || convLayers[i][0] == 0) {
			return;
		}

		// Compute size of matrix after convolution
		intermediarySize[0] = intermediarySize[0] - convLayers[i][0] + 1;
		intermediarySize[1] = intermediarySize[1] - convLayers[i][1] + 1;

		if(intermediarySize[0] <= 0 || intermediarySize[1] <= 0) {
			return;
		}
	}

	// Make sure layers are not of size 0
	for(unsigned i=0; i<denseLayers.size(); i++) {
		if(denseLayers[i] == 0) {
			return;
		}
	}


		// Make sure pooling is possible
	if(
		poolingSize[0] % intermediarySize[0] != 0 ||
		poolingSize[1] % intermediarySize[1] != 0
	) {
		return;
	}


		// Randomly init kernels, weights and biases

	// Convolution layers
	for(unsigned i=0; i<convLayers.size(); i++) {
		convKernels.push_back(ts::Tensor<T>(
			Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic>()
			.setRandom(convLayers[i][0], convLayers[i][1]),
			&(this->wList))
		);
	}

	// Fully connected layers
	denseLayers.insert(
		// First dense layer input will be flattened convolution output
		denseLayers.begin(), intermediarySize[0] * intermediarySize[1]
	);

	for(unsigned i=1; i<denseLayers.size(); i++) {
		// Add layer i-1 -> layer i weights
		weights.push_back(ts::Tensor<T>(
			Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic>()
			.setRandom(denseLayers[i], denseLayers[i-1]),
			&(this->wList))
		);

		// Add layer i biases
		biases.push_back(ts::Tensor<T>(
			Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic>()
			.setRandom(denseLayers[i], 1),
			&(this->wList))
		);
	}

	// Set up data fields
	expectedInput = inputSize;
	pooling = poolingSize;

}



template <typename T>
void ts::ConvolutionalNetwork<T>::toggleGlobalOptimize(bool enable) {
	if(weights.size() != biases.size()) {
		return;
	}

	for(unsigned i=0; i<convKernels.size(); i++) {
		this->toggleOptimize(&(convKernels[i]), enable);
	}

	for(unsigned i=0; i<weights.size(); i++) {
		this->toggleOptimize(&(weights[i]), enable);
		this->toggleOptimize(&(biases[i]), enable);
	}
}



template <typename T>
ts::Tensor<T> ts::ConvolutionalNetwork<T>::compute(ts::Tensor<T> input) {

	// NOTE It might be a good idea to add an entire function to make sure that
	// all parameters are compatible (in terms of size), and that output is
	// computable

	// Make sure input is large enough for first convolution
	if(
	input.getValue().rows() != expectedInput[0] ||
	input.getValue().cols() != expectedInput[1]
	) {
		return ts::Tensor<T>(
			Eigen::Array<T, 0, 0>(),
			&(this->wList)
		);
	}

	// 1) Convolution computation loop
	for(unsigned i=0; i<convKernels.size(); i++) {
		input = (*activationFunction)(convolution(input, convKernels[i]));
		input = maxPooling(input, pooling);
	}

	// 2) Flatten convolution output
	input = flattening(input);

	// 3) Dense layers computation loop
	for(unsigned i=0; i<weights.size(); i++) {
		input = (*activationFunction)(matProd(weights[i], input) + biases[i]);
	}

	return input;
}



template <typename T>
void ts::ConvolutionalNetwork<T>::save(std::string filePath) {
	std::ofstream out(filePath);

	out << ts::serializeTensorsVector(convKernels);
	out << ts::serializeTensorsVector(weights);
	out << ts::serializeTensorsVector(biases);

	out.close();
}



template <typename T>
void ts::ConvolutionalNetwork<T>::load(std::string filePath) {
	// Delete current tensors and reset wList
	convKernels = {};
	weights = {};
	biases = {};
	this->wList.reset();

	// Load new tensors
	std::ifstream in(filePath);

	convKernels = ts::parseTensorsVector(in, &(this->wList));
	weights = ts::parseTensorsVector(in, &(this->wList));
	biases = ts::parseTensorsVector(in, &(this->wList));

	in.close();
}
