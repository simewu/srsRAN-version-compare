#include "handshake_proof_merklecpp.h"
#include "openssl/sha.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <vector>

// Computes the SHA-256 hash of a string
std::string sha256(const std::string str)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, str.c_str(), str.size());
    SHA256_Final(hash, &sha256);
    std::stringstream ss;
    for(int i = 0; i < SHA256_DIGEST_LENGTH; i++)
    {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}

// The function used to sort the vector of file names
bool pathCompareFunction (std::string a, std::string b) {
	return a < b;
}

// Recursively list the files in a directory
std::vector<std::string> getFiles(std::string directory, std::string regexToIncludeStr, std::string regexToIgnoreStr) {
	std::vector<std::string> files;
	for(std::filesystem::recursive_directory_iterator i(directory), end; i != end; ++i) {
		if(!is_directory(i->path())) {
			std::string str = i->path();
			if(std::regex_match(str, std::regex(regexToIncludeStr)) && !std::regex_match(str, std::regex(regexToIgnoreStr))) {
				files.push_back(str);
			}
		}
	}
	std::cout << "Sorting files..." << std::endl;
	std::sort(files.begin(),files.end(), pathCompareFunction);
	// for(int i = 0; i < files.size(); i++) {
	// 	std::cout << "Including \"" << files.at(i) << "\"" << std::endl;
	// }
	return files;
}

// Read the contents of a file at a given file path
std::string getContents(std::string filePath) {
	std::string contents = "";
	std::ifstream f(filePath);
	while(f) {
		std::string line;
		getline(f, line);
		contents += line + '\n';
	}
	return contents;
}

// Given a number (e.g. 10) compute the next power of two (e.g. 16)
int nextPowerOfTwo(int num) {
	double n = log2(num);
	return (int)pow(2, ceil(n));
}

// Update the hash at an index within the tree
void updateHashAtIndex(merkle::Tree &tree, int index, std::string hash_string) {
	merkle::TreeT<32, merkle::sha256_compress>::Node* ID = tree.walk_to(index, true, [](merkle::TreeT<32, merkle::sha256_compress>::Node* n, bool go_right) {
		n->dirty = true;
        return true;
    });
    merkle::Tree::Hash newHash(hash_string);
    ID->hash = newHash;
	tree.compute_root();
}

int main() {
	std::vector<std::string> directories;
	directories.push_back("srsRAN-release_002_000_000");
	directories.push_back("srsRAN-release_17_09");
	directories.push_back("srsRAN-release_17_12");
	directories.push_back("srsRAN-release_18_03");
	directories.push_back("srsRAN-release_18_03_1");
	directories.push_back("srsRAN-release_18_06");
	directories.push_back("srsRAN-release_18_06_1");
	directories.push_back("srsRAN-release_18_09");
	directories.push_back("srsRAN-release_18_12");
	directories.push_back("srsRAN-release_19_03");
	directories.push_back("srsRAN-release_19_06");
	directories.push_back("srsRAN-release_19_09");
	directories.push_back("srsRAN-release_19_12");
	directories.push_back("srsRAN-release_20_04");
	directories.push_back("srsRAN-release_20_04_1");
	directories.push_back("srsRAN-release_20_04_2");
	directories.push_back("srsRAN-release_20_10");
	directories.push_back("srsRAN-release_20_10_1");
	directories.push_back("srsRAN-release_21_04_pre");
	directories.push_back("srsRAN-release_21_04_zzz");
	directories.push_back("srsRAN-release_21_10");
	directories.push_back("srsRAN-release_22_04");

	for(int d = 0; d < directories.size(); d++) {
		std::string directory = "../" + directories.at(d);
		std::cout << "Processing directory \"" << directory << "\"..." << std::endl;

		std::string regexToIncludeStr = ".*(\\.cpp|\\.c|\\.h|\\.cc|\\.py|\\.sh|\\.go|\\.js|\\.java)";
		std::string regexToIgnoreStr = "";

		// Get the list of code file names
		std::vector<std::string> files = getFiles(directory, regexToIncludeStr, regexToIgnoreStr);
		std::vector<std::string> hashes (files.size());
		// Compute the hash of the files
		for(int i = 0; i < files.size(); i++) {
			//totalBytes += getContents(files.at(i)).length();
			hashes.at(i) = sha256(getContents(files.at(i)));

			//std::cout << "File \"" << files.at(i) << "\" has has \"" << hashes.at(i) << "\"" << std::endl;
		}
		// Set the initial ID
		hashes.insert(hashes.begin(), "0000000000000000000000000000000000000000000000000000000000000000");
		// Adjust the size to make it a full binary tree
		int targetSize = nextPowerOfTwo(hashes.size()), i = 0;
		while(hashes.size() < targetSize) {
			hashes.push_back(hashes.at(i));
			i++;
		}

		// Cybersecurity Lab: Testing a mini tree
		// std::vector<std::string> hashes (16);
		// for(int i = 0; i < 16; i++) {
		// 	hashes.at(i) = sha256(to_string(i + 1));
		// }

		// Convert the hashes to Merkle node objects
		std::vector<merkle::Tree::Hash> leaves (hashes.size());
		for(int i = 0; i < hashes.size(); i++) {
			merkle::Tree::Hash hash(hashes.at(i));
			leaves.at(i) = hash;
		}

		// Create the tree
		merkle::Tree tree;
		tree.insert(leaves);

		// Update the ID
		updateHashAtIndex(tree, 0, "0000000000000000000000000000000000000000000000000000000000000000");

		if(tree.root().to_string() == "db690426d6b029f9cf116e4b15895ef8105564762fd49408e026cc04fc579f4e") {
			std::cout << "Correct version!" << std::endl;
		} else {
			std::cout << "Incorrect version: " << tree.root().to_string() << std::endl;
		}

		//std::cout << "Total file bytes: " << totalBytes << std::endl;
		std::cout << "Total tree bytes: " << tree.to_string().length() << std::endl;

		// auto root = tree.root();
		// auto path = tree.path(0);
		// assert(path->verify(root));

	}
	return 0;
}