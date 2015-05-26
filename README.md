Synergy cryptocurrency [SNRG].

Synergy is a peer-to-peer decentralized cryptocurrency that
uses a dual-algorithm system to mine blocks. For the first 10
days, which contains the full PoW (proof of work) period,
Synergy uses the X11 algorithm. Because X11 is unnecessarily
inefficient after PoW, Synergy switches to SHA256d for
the PoS period (proof of stake). SHA256d
is easier on CPUs during syncronization and bootstrap
because it requires only one hash to
verify a block whereas X11 requires 11 hashes.

The early part of the Synergy PoS period makes use
of Turbo Stake, which awards Synergy holders greater
interest for every stake. The multiplier is directly
used in the reward calculation by multiplying it
with the base Synergy interest rate of 10% per year.

Holders build the Turbo
Stake multiplier over two days by staking consistently.
After that, the multiplier will level out and the
holder will stake with a consistent rate that depends
on how much stake competes with his. The Turbo Stake
period lasts 30 days from the time of launch.

Synergy supports disabling staking with the setting
"staking=0".

* Ticker Symbol: SNRG
* PoW Algorithm: X11
* PoS Algorithm: X11, switching to SHA256d after 10 days
* RPC Port: 50542 (configurable with rpcport= option)
* P2P Port: 40698 (configurable with port= option)
* Tor Port: 38155 (configurable with torport= option)
* Block Times: 2 Minutes
* PoW Blocks: 4320 (6 days)
* 10% POS Interest per Year
* Max Turbo Stake Multiplier: 288
* Turbo Stake Lookback: 2 days
* Percent of Blocks over 2 days for Max Multiplier: 20%
* Max Money Supply after PoW: 250,001 SNRG
* Stake Minumum Age: 48 hours (2 days)
* Stake Maximum Age: 144 hours (6 days)
* Stake Maximum Reward Age: 8 days
* New Mint Spendable: 120 blocks (4 hours)
* Message Start ("Magic Bytes"): 0xf1, 0xe3, 0xe5, 0xd9
* Message Start Test Net: 0xaf, 0xb9, 0xd9, 0xff
* Application Data Folder
     - Windows: "Synergy"
     - OS X: "Synergy"
     - Linux: ".synergy"
* Config File Name
     - Windows: "synergy.conf"
     - OS X: "synergy.conf"
     - Linux: "synergy.conf"
