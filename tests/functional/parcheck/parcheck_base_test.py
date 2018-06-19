nzbget_options = ['ParCheck=auto', 'ParQuick=yes', 'PostStrategy=sequential']

def test_parchecker_healthy(nserv, nzbget):
	hist = nzbget.download_nzb('parchecker2.nzb')
	assert hist['Status'] == 'SUCCESS/HEALTH'

def test_parchecker_repair(nserv, nzbget):
	nzb_content = nzbget.load_nzb('parchecker2.nzb')
	nzb_content = nzb_content.replace('parchecker2/testfile.7z.001?16=45000:3000', 'parchecker2/testfile.7z.001?16=45000:3000!0')
	hist = nzbget.download_nzb('parchecker.repair.nzb', nzb_content)
	assert hist['Status'] == 'SUCCESS/PAR'

def test_parchecker_subject(nserv, nzbget):
	nzb_content = nzbget.load_nzb('parchecker2.nzb')
	nzb_content = nzb_content.replace('parchecker2/testfile.7z.001?16=45000:3000', 'parchecker2/testfile.7z.001?16=45000:3000!0')
	nzb_content = nzb_content.replace('subject="&quot;', 'subject="')
	nzb_content = nzb_content.replace('&quot; yEnc', '.dat yEnc')
	hist = nzbget.download_nzb('parchecker.subject.nzb', nzb_content)
	assert hist['Status'] == 'SUCCESS/PAR'

def test_parchecker_middle(nserv, nzbget):
	nzb_content = nzbget.load_nzb('parchecker2.nzb')
	nzb_content = nzb_content.replace('parchecker2/testfile.7z.001?16=45000:3000', 'parchecker2/testfile.7z.001?16=45000:3000!0')
	hist = nzbget.download_nzb('parchecker.middle.nzb', nzb_content)
	assert hist['Status'] == 'SUCCESS/PAR'

def test_parchecker_last(nserv, nzbget):
	nzb_content = nzbget.load_nzb('parchecker2.nzb')
	nzb_content = nzb_content.replace('<segment bytes="3000" number="18">parchecker2/testfile.7z.001?18=51000:200</segment>', '')
	hist = nzbget.download_nzb('parchecker.last.nzb', nzb_content, unpack=False)
	assert hist['Status'] == 'SUCCESS/HEALTH'

def test_parchecker_last_unpack(nserv, nzbget):
	nzb_content = nzbget.load_nzb('parchecker2.nzb')
	nzb_content = nzb_content.replace('<segment bytes="3000" number="18">parchecker2/testfile.7z.001?18=51000:200</segment>', '')
	hist = nzbget.download_nzb('parchecker.last.unpack.nzb', nzb_content, unpack=True)
	assert hist['Status'] == 'SUCCESS/UNPACK'
