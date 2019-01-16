nzbget_options = ['ParRename=no', 'RarRename=no', 'ParCheck=manual', 'DirectRename=no']

def test_rename_obf1(nserv, nzbget):
	hist = nzbget.download_nzb('obfuscated1.nzb', unpack=True)
	assert hist['Status'] == 'WARNING/DAMAGED'

def test_rename_obf1ch(nserv, nzbget):
	nzb_content = nzbget.load_nzb('obfuscated1.nzb')
	nzb_content = nzb_content.replace(';5mb.7z', ';abc')
	nzb_content = nzb_content.replace(';parrename.par2', ';def')
	nzb_content = nzb_content.replace('.par2&', '&')
	hist = nzbget.download_nzb('obfuscated1-changed.nzb', nzb_content, unpack=True)
	assert hist['Status'] == 'WARNING/DAMAGED'

def test_rename_obf2(nserv, nzbget):
	hist = nzbget.download_nzb('obfuscated2.nzb', unpack=True)
	assert hist['Status'] == 'SUCCESS/HEALTH'

def test_parjoin_1(nserv, nzbget):
	hist = nzbget.download_nzb('parjoin1.nzb', unpack=True)
	assert hist['Status'] == 'SUCCESS/UNPACK'

def test_parjoin_2(nserv, nzbget):
	hist = nzbget.download_nzb('parjoin2.nzb', unpack=True)
	assert hist['Status'] == 'WARNING/DAMAGED'

def test_parjoin_3(nserv, nzbget):
	hist = nzbget.download_nzb('parjoin3.nzb', unpack=True)
	assert hist['Status'] == 'WARNING/DAMAGED'
