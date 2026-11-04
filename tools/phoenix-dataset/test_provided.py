import hashlib,json,unittest
from pathlib import Path
class ProvidedBatch(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rows=[json.loads(x) for x in Path('research/phoenix-reference-dataset/dataset_manifest.jsonl').read_text(encoding='utf-8').splitlines()]
    def test_original_bytes_unchanged(self):
        source=Path('research/phoenix_reference_fetch_kit/phoenix_reference_fetch/downloaded/images')
        selected=[r for r in self.rows if r['id'].startswith(('P1_','P2_'))]
        self.assertEqual(len(selected),22)
        for r in selected:
            original=next(source.rglob(r['id']+'.*'))
            self.assertEqual(hashlib.sha256(original.read_bytes()).hexdigest(),r['sha256'])
            self.assertEqual(hashlib.sha256(Path(r['local_file']).read_bytes()).hexdigest(),r['sha256'])
    def test_actual_usable_counts_and_duplicates(self):
        selected=[r for r in self.rows if 'color_exploratory' in r.get('allowed_uses',[])]
        for stock in ['Phoenix 200','Phoenix II 200']:
            self.assertEqual(sum(r['film_stock']==stock for r in selected),16)
        self.assertEqual(len({r['sha256'] for r in selected}),32)
        self.assertFalse(any(r.get('visual_duplicate_of') for r in selected))
        self.assertTrue(all(r.get('identity_verified') for r in selected))
    def test_masks_and_fit_gate(self):
        self.assertFalse(any(r.get('accepted_for_fitting') for r in self.rows))
        for r in self.rows:
            for mask in r.get('statistics_mask',[]):
                x0,y0,x1,y1=mask['rect_normalized']
                self.assertTrue(0<=x0<x1<=1 and 0<=y0<y1<=1)
if __name__=='__main__':unittest.main()
