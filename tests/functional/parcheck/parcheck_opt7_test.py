nzbget_options = ['ParCheck=auto', 'ParQuick=no', 'PostStrategy=rocket']

def test_parchecker_healthy(nserv, nzbget):
	hist = nzbget.download_nzb('parchecker2.nzb')
	assert hist['Status'] == 'SUCCESS/HEALTH'

def test_parchecker_repair(nserv, nzbget):
	nzbget.api.pausepost();
	nzb_content = nzbget.load_nzb('parchecker2.nzb')
	nzb_content = nzb_content.replace('parchecker2/testfile.7z.001?16=45000:3000', 'parchecker2/testfile.7z.001?16=45000:3000!0')
	nzbget.append_nzb('parchecker.1.nzb', nzb_content, dupemode='FORCE')
	nzbget.append_nzb('parchecker.2.nzb', nzb_content, dupemode='FORCE')
	nzbget.append_nzb('parchecker.3.nzb', nzb_content, dupemode='FORCE')

	while True:
		status = nzbget.api.status()
		if status['RemainingSizeMB'] == 0:
			break
		time.sleep(0.1)

	nzbget.api.resumepost();
	hist1 = nzbget.wait_nzb('parchecker.3.nzb');
	hist2 = nzbget.wait_nzb('parchecker.3.nzb');
	hist3 = nzbget.wait_nzb('parchecker.3.nzb');
	assert hist1['Status'] == 'SUCCESS/PAR'
	assert hist2['Status'] == 'SUCCESS/PAR'
	assert hist3['Status'] == 'SUCCESS/PAR'
