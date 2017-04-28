nzbget_options = ['ParRename=no', 'RarRename=no', 'ParCheck=auto', 'DirectRename=yes']

def test_rename_obf1(nserv, nzbget):
	hist = nzbget.download_nzb('obfuscated1.nzb', unpack=True)
	assert hist['Status'] == 'SUCCESS/UNPACK'

def test_rename_obf1ch(nserv, nzbget):
	nzb_content = nzbget.load_nzb('obfuscated1.nzb')
	nzb_content = nzb_content.replace(';5mb.7z', ';abc')
	nzb_content = nzb_content.replace(';parrename', ';def')
	nzb_content = nzb_content.replace('.par2&', '&')
	hist = nzbget.download_nzb('obfuscated1-changed.nzb', nzb_content, unpack=True)
	assert hist['Status'] == 'SUCCESS/UNPACK'

def test_rename_obf2(nserv, nzbget):
	hist = nzbget.download_nzb('obfuscated2.nzb', unpack=True)
	assert hist['Status'] == 'SUCCESS/UNPACK'

def test_rename_obf3(nserv, nzbget):
	hist = nzbget.download_nzb('obfuscated3.nzb', unpack=True)
	assert hist['Status'] == 'SUCCESS/UNPACK'

def test_rename_obf1_damaged(nserv, nzbget):
	nzb_content = nzbget.load_nzb('obfuscated1.nzb')
	nzb_content = nzb_content.replace('abc.01?4=300000:100000', 'abc.01?4=300000:100000!0')
	hist = nzbget.download_nzb('obfuscated1-damaged.nzb', nzb_content, unpack=True)
	assert hist['Status'] == 'SUCCESS/UNPACK'

def test_rename_obf3_damaged(nserv, nzbget):
	nzb_content = nzbget.load_nzb('obfuscated3.nzb')
	nzb_content = nzb_content.replace('abc.01?17=1600000:100000', 'abc.01?17=1600000:100000!0')
	hist = nzbget.download_nzb('obfuscated3-damaged.nzb', nzb_content, unpack=True)
	assert hist['Status'] == 'SUCCESS/UNPACK'
