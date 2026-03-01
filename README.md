# message-from-kabira

<img width="240" height="320" alt="image" src="https://github.com/user-attachments/assets/5b7a297a-750f-4129-8767-32517dddea72" />


This is a project that turns an esp32-s3 into a digital poet. it broadcasts short poems and quotes to people's phones through their wi-fi settings. no app needed.
how it works: phones constantly look for wi-fi networks and broadcast their mac addresses. this board listens for those signals. when it detects a nearby phone, it takes that mac address and uses it to generate text using a markov chain. the text generation is trained on the poetry and dohas of kabirdas.

after generating the text, the board instantly creates a few fake wi-fi networks. when the person looks at their phone's wi-fi menu, the network names form a short message or poem.


![Untitled](https://github.com/user-attachments/assets/e578e333-6f2b-4b20-bb3a-6d607aa52c4a)
<img width="320" height="240" alt="image" src="https://github.com/user-attachments/assets/226a2875-6eba-4982-8295-ab06ea21a2a1" />


### how it works

the whole thing runs directly on the esp32-s3. the 8mb psram easily holds the markov chain data. it doesn't need the internet to work. it just uses wifi promiscuous mode to read mac addresses and then switches to access point mode to broadcast the text.
<img width="1366" height="768" alt="image" src="https://github.com/user-attachments/assets/46708437-0cab-47c2-8e3f-eaf224ea8347" />
