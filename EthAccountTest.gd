extends Label

func _hd_wallet_import_test():
	var address_const = [
	"c75185ff30635988d4ae44ab544fc66130932568",
	"fce4b4e710f93dbbb219555f71a62b4cebcfa907",
	"f85ccde06eab0391d976efff4d9de02eca09c566",
	"fb30288ec7c57819547b9b0f3cdf3941cad66790",
	"dee895839eb69fe79336c76b20045f6ca2f37f6a"
]
	var ethMgr = EthWalletManager.new()
	var m = ["oil", "bamboo", "reject", "omit", "gentle", "boss", "useless", "fog", "genuine", "primary", "divorce", "abstract"]
	var wallet = ethMgr.from_mnemonic(m)
	var counts = 5
	for index in counts:
		wallet.add()
	
	var accounts = wallet.get_accounts() 

	if accounts.size() > 0:
		for i in range(accounts.size()):
			var account = accounts[i]
		# 确保 account 是 EthAccount 类型
			if account is EthAccount:
				var eth_account = account as EthAccount  # 转换为 EthAccount
				var address = eth_account.get_address()  # 调用 EthAccount 的方法
				assert(address.hex_encode() == address_const[i])  # 使用索引检查是否匹配
				print("Account Address:", address.hex_encode())
			else:
				print("Object at index", i, "is not of type EthAccount")
	else:
		print("No accounts found in the wallet.")
		
	pass

func _hd_wallet_create_test():
	
	var ethMgr = EthWalletManager.new()
	var counts = 5
	var wallet = ethMgr.create(counts)
	print("HD Wallet Mnemonic : ", wallet.get_mnemonic())
	for index in counts:
		wallet.add()
	
	var accounts = wallet.get_accounts() 

	if accounts.size() > 0:
		for i in range(accounts.size()):
			var account = accounts[i]
		# 确保 account 是 EthAccount 类型
			if account is EthAccount:
				var eth_account = account as EthAccount  # 转换为 EthAccount
				var address = eth_account.get_address()  # 调用 EthAccount 的方法
				print("Account Address:", address.hex_encode())
			else:
				print("Object at index", i, "is not of type EthAccount")
	else:
		print("No accounts found in the wallet.")
	pass

# Called when the node enters the scene tree for the first time.
func _ready():
	_hd_wallet_import_test();
	print("--------------------------------------------------------------")
	_hd_wallet_create_test();

	pass # Replace with function body.


# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta):
	pass