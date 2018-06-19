nzbget_options = ['ParCheck=auto', 'ParQuick=yes', 'PostStrategy=balanced']

def test_parchecker_healthy(nserv, nzbget):
	hist = nzbget.download_nzb('parchecker2.nzb')
	assert hist['Status'] == 'SUCCESS/HEALTH'

def test_parchecker_repair(nserv, nzbget):
	nzb_content = nzbget.load_nzb('parchecker2.nzb')
	nzb_content = nzb_content.replace('parchecker2/testfile.7z.001?16=45000:3000', 'parchecker2/testfile.7z.001?16=45000:3000!0')
	hist = nzbget.download_nzb('parchecker.repair.nzb', nzb_content)
	assert hist['Status'] == 'SUCCESS/PAR'
