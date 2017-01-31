/*! The class that represents a tree used to build a Huffman table.
This file is part of https://github.com/kandrosov/OnChipDataCompression. */

#pragma once

#include <queue>
#include <boost/bimap.hpp>

#include "HuffmanLetterCode.h"

namespace pixel_studies {

template<typename Letter, typename Integer>
class HuffmanTree {
public:
    using LetterFrequencyMap = std::map<Letter, Integer>;
    using HuffmanTable = boost::bimap<Letter, HuffmanCode>;

private:
    struct Node;
    typedef std::shared_ptr<Node> PNode;
    typedef std::pair<PNode, PNode> PNodePair;
    typedef std::map<PNode, Letter> NodeLetterMap;

    struct Node {
        PNodePair daughters;
        Integer frequency;

        Node() : frequency(0) {}
        Node(Integer _frequency) : frequency(_frequency) {}
        Node(const PNode& first_daughter, const PNode& second_daughter)
            : daughters(first_daughter, second_daughter),
              frequency(first_daughter->frequency + second_daughter->frequency) {}
    };

public:
    HuffmanTree(const LetterFrequencyMap& _letter_frequencies)
        : letter_frequencies(_letter_frequencies)
    {
        static const auto cmp = [](const PNode& first, const PNode& second) {
            return first->frequency > second->frequency;
        };

        std::priority_queue<PNode, std::vector<PNode>, decltype(cmp)> node_queue(cmp);
        for(const auto& entry : letter_frequencies) {
            const Integer frequency = std::max<Integer>(1, entry.second);
            const PNode node(new Node(frequency));
            letter_nodes[node] = entry.first;
            node_queue.push(node);
        }

        while(node_queue.size() > 1) {
            const PNode first = node_queue.top();
            node_queue.pop();
            const PNode second = node_queue.top();
            node_queue.pop();
            const PNode cmb(new Node(first, second));
            node_queue.push(cmb);
        }

        root_node = node_queue.top();
        BuildTable(root_node);

    }

    const HuffmanTable& GetTable() const { return table; }

private:
    void BuildTable(const PNode& node, const HuffmanCode& code = HuffmanCode())
    {
        if(letter_nodes.count(node)) {
            const Letter& letter = letter_nodes.at(node);
            table.insert({ letter, code });
        } else {
            BuildTable(node->daughters.first, HuffmanCode(code, false));
            BuildTable(node->daughters.second, HuffmanCode(code, true));
        }
    }

private:
    LetterFrequencyMap letter_frequencies;
    NodeLetterMap letter_nodes;
    PNode root_node;
    HuffmanTable table;
};

} // namespace pixel_studies
