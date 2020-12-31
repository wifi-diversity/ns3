# ns3
ns3 code used in the paper "Attention to Wi-Fi Diversity: Resource Management in WLANs with Heterogeneous APs", IEEE Access, Jan 2021

ns3 scripts for simulating scenarios in which 802.11 scenarios with different kinds of APs are studied.

A previous version of this study can be found here: https://github.com/wifi-sweet-spot/ns3


## Citation
If you use this code, please cite the next research article:

Jose Saldana1, José Ruiz-Mas, Julián Fernández-Navajas, José Luis Salazar, Jean-Philippe Javaudin, Jean-Michel Bonnamy, and Maël Le Dizes, "Attention to Wi-Fi Diversity: Resource Management in WLANs with Heterogeneous APs," IEEE Access accepted for publication, Jan. 2021.

http://ieeexplore.ieee.org/document/ PENDING

It is an Open Access article.

## Content of the repository

The `.cc` folder contains the ns3 scripts. They have been run with ns3-30.1 (https://www.nsnam.org/releases/ns-3-30/).

`wifi-central-controlled-aggregation_v261.cc` is the main ns3 file used for the paper.

`wifi-central-controlled-aggregation_v259.cc` has also been used.


The folder `sh` contains the files used for obtaining each of the figures presented in the paper.

- Figure 15 was obtained with `test_lvap_013.sh`.

- Figure 16 was obtained with `test_lvap_005.sh` and `test_lvap_006.sh`.

- Figure 17 was obtained with `test_lvap_013.sh` and `test_lvap_014.sh`.

- Figure 18 was obtained with `test_lvap_011.sh` and `test_lvap_012.sh`.

- Figure 19 was obtained with `test_lvap_015.sh` and `test_lvap_016.sh`.


```

## How to use it

- Download ns3.

- Put the `.cc` file in the `ns-3.30.1/scratch` directory.

- Put a `.sh` file in the `ns-3.30.1` directory.

- Run a `.sh` file. (You may need to adjust the name of the `.cc` file in the `.sh` script).
