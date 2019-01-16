import re

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

def test_rename_obf1dm(nserv, nzbget):
	nzb_content = nzbget.load_nzb('obfuscated1.nzb')
	nzb_content = nzb_content.replace('abc.01?4=300000:100000', 'abc.01?4=300000:100000!0')
	hist = nzbget.download_nzb('obfuscated1-damaged.nzb', nzb_content, unpack=True)
	assert hist['Status'] == 'SUCCESS/UNPACK'

def test_rename_obf1dmf(nserv, nzbget):
	nzb_content = nzbget.load_nzb('obfuscated1.nzb')
	nzb_content = nzb_content.replace('abc.01?1=0:100000', 'abc.01?1=0:100000!0')
	hist = nzbget.download_nzb('obfuscated1-damaged-first.nzb', nzb_content, unpack=True)
	assert hist['Status'] == 'SUCCESS/UNPACK'

def test_rename_obf1dmf2(nserv, nzbget):
	nzb_content = nzbget.load_nzb('obfuscated1.nzb')
	nzb_content = nzb_content.replace('abc.01?1=0:100000', 'abc.01?1=0:100000!0')
	nzb_content = re.sub(r'abc.00\?1=0:\d+', r'\g<0>!0', nzb_content)
	hist = nzbget.download_nzb('obfuscated1-damaged-first2.nzb', nzb_content, unpack=True)
	assert hist['Status'] == 'SUCCESS/UNPACK'

def test_rename_obf1dmp(nserv, nzbget):
	nzb_content = nzbget.load_nzb('obfuscated1.nzb')
	nzb_content = re.sub(r'parrename.vol0+1.par2\?1=0:\d+', r'\g<0>!0', nzb_content)
	hist = nzbget.download_nzb('obfuscated1-damaged-par.nzb', nzb_content, unpack=True)
	assert hist['Status'] == 'SUCCESS/UNPACK'

def test_rename_obf3dm(nserv, nzbget):
	nzb_content = nzbget.load_nzb('obfuscated3.nzb')
	nzb_content = nzb_content.replace('abc.01?17=1600000:100000', 'abc.01?17=1600000:100000!0')
	hist = nzbget.download_nzb('obfuscated3-damaged.nzb', nzb_content, unpack=True)
	assert hist['Status'] == 'SUCCESS/UNPACK'

def test_rename_obf3dmf(nserv, nzbget):
	nzb_content = nzbget.load_nzb('obfuscated3.nzb')
	nzb_content = nzb_content.replace('abc.01?11=0:100000', 'abc.01?11=0:100000!0')
	hist = nzbget.download_nzb('obfuscated3-damaged-first.nzb', nzb_content, unpack=True)
	assert hist['Status'] == 'SUCCESS/UNPACK'

def test_rename_obf3dmf2(nserv, nzbget):
	nzb_content = nzbget.load_nzb('obfuscated3.nzb')
	nzb_content = nzb_content.replace('abc.01?11=0:100000', 'abc.01?11=0:100000!0')
	nzb_content = re.sub(r'abc.00\?1=0:\d+', r'\g<0>!0', nzb_content)
	hist = nzbget.download_nzb('obfuscated3-damaged-first2.nzb', nzb_content, unpack=True)
	assert hist['Status'] == 'WARNING/HEALTH'

def test_renameparchecker_healthy(nserv, nzbget):
	hist = nzbget.download_nzb('parchecker2.nzb')
	assert hist['Status'] == 'SUCCESS/HEALTH'

def test_parchecker_repair(nserv, nzbget):
	nzb_content = nzbget.load_nzb('parchecker2.nzb')
	nzb_content = nzb_content.replace('parchecker2/testfile.7z.001?16=45000:3000', 'parchecker2/testfile.7z.001?16=45000:3000!0')
	hist = nzbget.download_nzb('parchecker.repair.nzb', nzb_content)
	assert hist['Status'] == 'SUCCESS/PAR'

def test_parchecker_dmp(nserv, nzbget):
	nzb_content = nzbget.load_nzb('parchecker2.nzb')
	nzb_content = nzb_content.replace('parchecker2/testfile.7z.par2?1=0:3000', 'parchecker2/testfile.7z.par2?1=0:3000!0')
	hist = nzbget.download_nzb('parchecker2.damagedpar.nzb', nzb_content)
	assert hist['Status'] == 'SUCCESS/HEALTH'
	for entry in nzbget.api.loadlog(hist['ID'], 0, 10000):
		assert entry['Kind'] != 'ERROR', entry['Text']

def test_parjoin_1(nserv, nzbget):
	hist = nzbget.download_nzb('parjoin1.nzb', unpack=True)
	assert hist['Status'] == 'SUCCESS/PAR'

def test_parjoin_2(nserv, nzbget):
	hist = nzbget.download_nzb('parjoin2.nzb', unpack=True)
	assert hist['Status'] == 'SUCCESS/PAR'

def test_parjoin_3(nserv, nzbget):
	hist = nzbget.download_nzb('parjoin3.nzb', unpack=True)
	assert hist['Status'] == 'SUCCESS/PAR'
