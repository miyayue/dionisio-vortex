---

kanban-plugin: board

---

## to do

- [ ] **modelo**
	- [ ] montar
- [ ] **apresentação**
	- [ ] introdução - explicar o projeto
	- [ ] jornada de desenvolvimento
	- [ ] dificuldades/desafios
	- [ ] aprendizados
	- [ ] agradecimento


## in progress

- [ ] **hardware** placa
	- [x] desenhar placa
	- [x] imprimir 
	- [ ] corroer
	- [ ] soldar
- [ ] **software** dashboard
	 - [ ] layout
	 - [ ] checar recebimento de dados
	 - [ ] checar envio de dados


## fixing

- [ ] **firmware**
	- [x] conexão wifi
	- [x] timestamp
	- [x] temperatura e umidade
	- [x] status da porta
	- [x] leitura RFID
	- [x] abertura da trava
	- [ ] alertas buzzer e LED
	- [x] status de conexão 
	- [ ] último acesso (tag ou remoto)
	- [ ] comunicação HTTP
	- [ ] montar payload


## complete

**Complete**
- [x] **identificar e separar componentes**
	- [x] protoboard
	- [x] microcontrolador ==🟣ESP32==
	- [x] sensor ==🟣DHT22==
	- [x] display LCD
	- [x] sensor magnético reed switch 
	- [x] leitor RFID
	- [x] trava solenoide
	- [x] relé 5V optoacoplado de 1 canal
	- [x] fonte 12V/2A
	- [x] módulo buck step-down 12V para 5V
	- [x] resistor ==🟣4.7kΩ/10kΩ==
- [x] **hardware** protótipo 
	- [x] montar
	- [x] testar




%% kanban:settings
```
{"kanban-plugin":"board","list-collapse":[false,false,false,false],"full-list-lane-width":false,"show-checkboxes":false,"new-line-trigger":"enter"}
```
%%