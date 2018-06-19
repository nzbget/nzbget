nzbget_options = ['ParCheck=manual', 'ParQuick=yes', 'PostStrategy=sequential']

def test_parchecker_healthy(nserv, nzbget):
	hist = nzbget.download_nzb('parchecker2.nzb')
	assert hist['Status'] == 'SUCCESS/HEALTH'

def test_parchecker_repair(nserv, nzbget):
	nzb_content = nzbget.load_nzb('parchecker2.nzb')
	nzb_content = nzb_content.replace('parchecker2/testfile.7z.001?16=45000:3000', 'parchecker2/testfile.7z.001?16=45000:3000!0')
	hist = nzbget.download_nzb('parchecker.repair.nzb', nzb_content)
	assert hist['Status'] == 'WARNING/DAMAGED'

def test_parchecker_middle(nserv, nzbget):
	nzb_content = nzbget.load_nzb('parchecker2.nzb')
	nzb_content = nzb_content.replace('<segment bytes="3000" number="16">parchecker2/testfile.7z.001?16=45000:3000</segment>', '')
	hist = nzbget.download_nzb('parchecker.middle.nzb', nzb_content)
	assert hist['Status'] == 'WARNING/DAMAGED'

def test_parchecker_last(nserv, nzbget):
	nzb_content = nzbget.load_nzb('parchecker2.nzb')
	nzb_content = nzb_content.replace('<segment bytes="3000" number="18">parchecker2/testfile.7z.001?18=51000:200</segment>', '')
	hist = nzbget.download_nzb('parchecker.last.nzb', nzb_content)
	assert hist['Status'] == 'SUCCESS/HEALTH'
