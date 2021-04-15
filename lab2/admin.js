const express = require("express");
const bodyParser = require("body-parser");
const {spawn} = require('child_process');
const {spawnSync} = require('child_process');
const {exec} = require('child_process');
let app = express();
app.use(bodyParser.urlencoded({extended: true}));
app.use(express.static('public'));

let data = [];
app.get("/admin", (req, res) => {
    let ip = req.headers['x-forwarded-for'] || req.connection.remoteAddress;
    let remote_ip = ip.split(":")[3];
    data = []
	let out = spawnSync("iptables", ["-L", "-v", "-x"]).stdout.toString();
	let lines = out.split('\n');
	let flag = 0;
	for(let i = 0; i < lines.length; i++){
		let fields = lines[i].trim().split(/\s+/, 9);
		if(fields[0] === 'Chain' && fields[1] === 'FORWARD'){
			flag = 1;
			continue;
		}
		if(flag === 1 && fields[0] === ''){
			break;
		}
		if(flag === 1 && fields[0] !== 'pkts'){
			let tmp = {pkts: fields[0], bytes: fields[1], src: fields[7], dest: fields[8], status: fields[2]}
			if((tmp.src.includes("10.42.0.") && !tmp.src.includes("/")) || (tmp.dest.includes("10.42.0.") && !tmp.dest.includes("/"))){
				data.push(tmp);
			}
		}
	}
    res.setHeader("Context-type", "text/html")
    console.log("length: ", data.length)
    res.send(
    	`
    	<html>
    		<head>
    			<link rel="stylesheet" type="text/css" media="screen" href="styles.css" />
    		</head>
    		<body>
    			<form action="ban" method="post" id="myform">
				<table>
					<thead>
						<tr>
							<th>Packets</th>
							<th>Bytes</th>
							<th>Source</th>
							<th>Destination</th>
							<th>Status</th>
							<th>Select</th>
						</tr>
					</thead>
					<tbody>
    	`
    	+ data.map((d, index) => {
    		if(index % 2 === 0){
    			return `<tr><td>${d.pkts}</td><td>${d.bytes}</td><td>${d.src}</td><td>${d.dest}</td><td rowspan="2">${d.status}</td><td rowspan="2"><input class="myCheckbox" type="checkbox" name="name_${index/2}" form="myform" /></td></tr><tr><td>${data[index+1].pkts}</td><td>${data[index+1].bytes}</td><td>${data[index+1].src}</td><td>${data[index+1].dest}</td></tr>`;
    		}
    	}).join('')
    	 + `
			 		</tbody>
			 	</table>
                <center><button>GO!</button></center>
             </form>
			 </body>
    	 </html>`
        );
});

app.post("/ban", (req, res) => {
	let c = req.body["name_1"];
	for(let i = 0; i < data.length / 2; i++){
		if(req.body["name_" + i.toString()] === "on"){
			let ip = data[i*2]["src"]
			if(ip === "anywhere"){
				ip = data[i*2]["dest"]
			}
			let status = data[i*2]["status"]
			spawnSync("iptables", ["-t", "nat", "-D", "PREROUTING", "-d", ip, "-j", status])
			spawnSync("iptables", ["-t", "nat", "-D", "PREROUTING", "-s", ip, "-j", status])
			console.log(ip)
			//spawn("iptables", ["-t", "nat", "-D", "PREROUTING", "-d", ip, "-j", "ACCEPT"])
			spawnSync("iptables", ["-D", "FORWARD", "-d", ip, "-j", status])
			spawnSync("iptables", ["-D", "FORWARD", "-s", ip, "-j", status])
			//spawn("iptables", ["-D", "FORWARD", "-d", ip, "-j", "ACCEPT"])
			if(status=== "ACCEPT"){
				spawnSync("iptables", ["-I", "FORWARD",  "-d", ip, "-j", "DROP"])
				spawnSync("iptables", ["-I", "FORWARD",  "-s", ip, "-j", "DROP"])
				//spawn("iptables", ["-I", "FORWARD", "-d", ip, "-j", "DROP"])
				spawnSync("iptables", ["-I", "INPUT", "-d", ip, "-j", "DROP"])
				spawnSync("iptables", ["-I", "INPUT", "-s", ip, "-j", "DROP"])
				//spawn("iptables", ["-I", "INPUT", "-d", ip, "-j", "DROP"])
			}
			else{
				//spawn("iptables", ["-I", "FORWARD", "-d", ip, "-j", "DROP"])
				spawnSync("iptables", ["-D", "INPUT", "-d", ip, "-j", "DROP"])
				spawnSync("iptables", ["-D", "INPUT", "-s", ip, "-j", "DROP"])
				//spawn("iptables", ["-I", "INPUT", "-d", ip, "-j", "DROP"])
			}
			
		}
	}
	res.redirect("back");
});

app.listen(48763);
console.log("Start listening!");
