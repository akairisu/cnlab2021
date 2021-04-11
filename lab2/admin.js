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
							<th>packets</th>
							<th>bytes</th>
							<th>source</th>
							<th>destination</th>
							<th>status</th>
							<th>ban?</th>
						</tr>
					</thead>
					<tbody>
    	`
    	+ data.map((d, index) =>
    	`<tr><td>${d.pkts}</td><td>${d.bytes}</td><td>${d.src}</td><td>${d.dest}</td><td>${d.status}</td><td><input type="checkbox" name="name_${index}" form="myform" /></td></tr>`).join('')
    	 + `
			 		</tbody>
			 	</table>
                which IP do you want to ban: <input type="text" name="banip" form="myform"/>
                </br>
                <button>GO!</button>
             </form>
			 </body>
    	 </html>`
        );
});

app.post("/ban", (req, res) => {
	let c = req.body["name_1"];
	console.log(c);
	for(let i = 0; i < data.length; i++){
		if(req.body["name_" + i.toString()] === "on"){
			let src = data[i]["src"]
			let dest = data[i]["dest"]
			let status = data[i]["status"]
			if(src === "anywhere"){
				spawn("iptables", ["-t", "nat", "-D", "PREROUTING", "-d", dest, "-j", status])
				console.log(src, dest)
				//spawn("iptables", ["-t", "nat", "-D", "PREROUTING", "-d", ip, "-j", "ACCEPT"])
				spawn("iptables", ["-D", "FORWARD", "-d", dest, "-j", status])
				//spawn("iptables", ["-D", "FORWARD", "-d", ip, "-j", "ACCEPT"])
				if(status=== "ACCEPT"){
					spawn("iptables", ["-I", "FORWARD",  "-d", dest, "-j", "DROP"])
					//spawn("iptables", ["-I", "FORWARD", "-d", ip, "-j", "DROP"])
					spawn("iptables", ["-I", "INPUT", "-d", dest, "-j", "DROP"])
					//spawn("iptables", ["-I", "INPUT", "-d", ip, "-j", "DROP"])
				}
				else{
					//spawn("iptables", ["-I", "FORWARD", "-d", ip, "-j", "DROP"])
					spawn("iptables", ["-D", "INPUT", "-d", dest, "-j", "DROP"])
					//spawn("iptables", ["-I", "INPUT", "-d", ip, "-j", "DROP"])
				}
			}
			if(dest === "anywhere"){
				spawn("iptables", ["-t", "nat", "-D", "PREROUTING", "-s", src, "-j", status])
				//spawn("iptables", ["-t", "nat", "-D", "PREROUTING", "-d", ip, "-j", "ACCEPT"])
				spawn("iptables", ["-D", "FORWARD", "-s", src, "-j", status])
				//spawn("iptables", ["-D", "FORWARD", "-d", ip, "-j", "ACCEPT"])
				if(status=== "ACCEPT"){
					spawn("iptables", ["-I", "FORWARD",  "-s", src, "-j", "DROP"])
					//spawn("iptables", ["-I", "FORWARD", "-d", ip, "-j", "DROP"])
					spawn("iptables", ["-I", "INPUT", "-s", src, "-j", "DROP"])
					//spawn("iptables", ["-I", "INPUT", "-d", ip, "-j", "DROP"])
				}
				else{
					//spawn("iptables", ["-I", "FORWARD", "-d", ip, "-j", "DROP"])
					spawn("iptables", ["-D", "INPUT", "-s", src, "-j", "DROP"])
					//spawn("iptables", ["-I", "INPUT", "-d", ip, "-j", "DROP"])
				}
			}
			
			console.log(i);
		}
	}
	res.redirect("back");
});

app.listen(48763);
console.log("Start listening!");
